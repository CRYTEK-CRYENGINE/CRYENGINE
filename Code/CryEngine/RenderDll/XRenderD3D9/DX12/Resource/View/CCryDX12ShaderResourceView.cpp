// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12ShaderResourceView.hpp"

#include "DX12/Device/CCryDX12Device.hpp"
#include "DX12/Resource/Misc/CCryDX12Buffer.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture2D.hpp"

CCryDX12ShaderResourceView* CCryDX12ShaderResourceView::Create(CCryDX12Device* pDevice, ID3D11Resource* pResource11, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc)
{
	ID3D12Resource* pResource12 = DX12_EXTRACT_D3D12RESOURCE(pResource11);

	// Special case: NullResource is valid when only the D3D12 resource is null
	if (!pResource11 && !pResource12)
	{
		DX12_ASSERT(0, "Unknown resource type!");
		return NULL;
	}

	TRange<uint16> slices(0, 1);
	TRange<uint16> mips(0, 1);

	D3D12_SHADER_RESOURCE_VIEW_DESC desc12;
	ZeroMemory(&desc12, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));

	desc12.Format = pDesc->Format;
	desc12.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc12.ViewDimension = static_cast<D3D12_SRV_DIMENSION>(pDesc->ViewDimension);
	switch (desc12.ViewDimension)
	{
	case D3D12_SRV_DIMENSION_BUFFER:
		desc12.Buffer.FirstElement = pDesc->Buffer.FirstElement;
		desc12.Buffer.NumElements = pDesc->Buffer.NumElements;
		desc12.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		desc12.Buffer.StructureByteStride = DX12_EXTRACT_BUFFER(pResource11)->GetStructureByteStride();
		break;
	case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
		desc12.Texture1DArray.ArraySize = pDesc->Texture1DArray.ArraySize;
		desc12.Texture1DArray.FirstArraySlice = pDesc->Texture1DArray.FirstArraySlice;
		desc12.Texture1DArray.MipLevels = pDesc->Texture1DArray.MipLevels;
		desc12.Texture1DArray.MostDetailedMip = pDesc->Texture1DArray.MostDetailedMip;
		desc12.Texture1DArray.ResourceMinLODClamp = 0.0f;
		slices.start = pDesc->Texture1DArray.FirstArraySlice;
		slices.end = slices.start + pDesc->Texture1DArray.ArraySize;
		mips.start = pDesc->Texture1DArray.MostDetailedMip;
		mips.end = mips.start + pDesc->Texture1DArray.MipLevels;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		desc12.Texture2DArray.ArraySize = pDesc->Texture2DArray.ArraySize;
		desc12.Texture2DArray.FirstArraySlice = pDesc->Texture2DArray.FirstArraySlice;
		desc12.Texture2DArray.MipLevels = pDesc->Texture2DArray.MipLevels;
		desc12.Texture2DArray.MostDetailedMip = pDesc->Texture2DArray.MostDetailedMip;
		desc12.Texture2DArray.PlaneSlice = ((pDesc->Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT) | (pDesc->Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT));
		desc12.Texture2DArray.ResourceMinLODClamp = 0.0f;
		slices.start = pDesc->Texture2DArray.FirstArraySlice;
		slices.end = slices.start + pDesc->Texture2DArray.ArraySize;
		mips.start = pDesc->Texture2DArray.MostDetailedMip;
		mips.end = mips.start + pDesc->Texture2DArray.MipLevels;
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
		desc12.TextureCubeArray.First2DArrayFace = pDesc->TextureCubeArray.First2DArrayFace;
		desc12.TextureCubeArray.MipLevels = pDesc->TextureCubeArray.MipLevels;
		desc12.TextureCubeArray.MostDetailedMip = pDesc->TextureCubeArray.MostDetailedMip;
		desc12.TextureCubeArray.NumCubes = pDesc->TextureCubeArray.NumCubes;
		desc12.TextureCubeArray.ResourceMinLODClamp = 0.0f;
		slices.start = pDesc->TextureCubeArray.First2DArrayFace;
		slices.end = slices.start + pDesc->TextureCubeArray.NumCubes * 6;
		mips.start = pDesc->Texture2DArray.MostDetailedMip;
		mips.end = mips.start + pDesc->Texture2DArray.MipLevels;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DMS:
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc12.Texture2DMSArray.FirstArraySlice = pDesc->Texture2DMSArray.FirstArraySlice;
		desc12.Texture2DMSArray.ArraySize = pDesc->Texture2DMSArray.ArraySize;
		slices.start = pDesc->Texture2DMSArray.FirstArraySlice;
		slices.end = slices.start + pDesc->Texture2DMSArray.ArraySize;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE1D:
		desc12.Texture1D.MipLevels = pDesc->Texture1D.MipLevels;
		desc12.Texture1D.MostDetailedMip = pDesc->Texture1D.MostDetailedMip;
		desc12.Texture1D.ResourceMinLODClamp = 0.0f;
		mips.start = pDesc->Texture1D.MostDetailedMip;
		mips.end = mips.start + pDesc->Texture1D.MipLevels;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2D:
		desc12.Texture2D.MipLevels = pDesc->Texture2D.MipLevels;
		desc12.Texture2D.MostDetailedMip = pDesc->Texture2D.MostDetailedMip;
		desc12.Texture2D.PlaneSlice = ((pDesc->Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT) | (pDesc->Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT));
		desc12.Texture2D.ResourceMinLODClamp = 0.0f;
		mips.start = pDesc->Texture2D.MostDetailedMip;
		mips.end = mips.start + pDesc->Texture2D.MipLevels;
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBE:
		desc12.TextureCube.MipLevels = pDesc->TextureCube.MipLevels;
		desc12.TextureCube.MostDetailedMip = pDesc->TextureCube.MostDetailedMip;
		desc12.TextureCube.ResourceMinLODClamp = 0.0f;
		slices.end = 6;
		mips.start = pDesc->TextureCube.MostDetailedMip;
		mips.end = mips.start + pDesc->TextureCube.MipLevels;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE3D:
		desc12.Texture3D.MipLevels = pDesc->Texture3D.MipLevels;
		desc12.Texture3D.MostDetailedMip = pDesc->Texture3D.MostDetailedMip;
		desc12.Texture3D.ResourceMinLODClamp = 0.0f;
		mips.start = pDesc->Texture3D.MostDetailedMip;
		mips.end = mips.start + pDesc->Texture3D.MipLevels;
		break;
	}

	auto& resource = DX12_EXTRACT_RESOURCE(pResource11)->GetDX12Resource();
	CD3DX12_RESOURCE_DESC descX(resource.GetDesc());

	const int planeCount = resource.GetPlaneCount();
	uint32 subresourceCount = descX.Subresources(planeCount);
	uint32 subresourcesInUse = D3D12CalcSubresourceCount(std::min(mips.Length(), descX.MipLevels), std::min(slices.Length(), descX.ArraySize()), planeCount);

	auto pSrv = DX12_NEW_RAW(CCryDX12ShaderResourceView(pResource11, *pDesc, pResource12, desc12));
	pSrv->GetDX12View().SetMapsFullResource(subresourcesInUse == subresourceCount);
	pSrv->GetDX12View().SetMips(mips);
	pSrv->GetDX12View().SetSlices(slices);

	return pSrv;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12ShaderResourceView::CCryDX12ShaderResourceView(ID3D11Resource* pResource11, const D3D11_SHADER_RESOURCE_VIEW_DESC& desc11, ID3D12Resource* pResource12, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc12)
	: Super(pResource11, NCryDX12::EVT_ShaderResourceView)
	, m_Desc11(desc11)
{
	m_DX12View.GetSRVDesc() = desc12;
}

/* ID3D11ShaderResourceView implementation */

void STDMETHODCALLTYPE CCryDX12ShaderResourceView::GetDesc(
  _Out_ D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc)
{
	if (pDesc)
	{
		*pDesc = m_Desc11;
	}
}
