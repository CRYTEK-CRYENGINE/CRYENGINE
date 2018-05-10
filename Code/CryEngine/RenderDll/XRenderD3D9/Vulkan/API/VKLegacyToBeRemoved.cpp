// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Functions that should be removed, but are required pending refactoring.

#include "StdAfx.h"
#include "VKMemoryResource.hpp"
#include "VKBufferResource.hpp"
#include "VKImageResource.hpp"
#include "VKResourceView.hpp"

namespace NCryVulkan
{

HRESULT CDeviceObject::SetPrivateData(REFGUID, UINT, const void*)
{
	return E_FAIL;
}

void CMemoryResource::GetType(D3D11_RESOURCE_DIMENSION* pDim)
{
	if (m_flags & kImageFlag1D)
	{
		*pDim = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
	}
	else if (m_flags & kImageFlag2D)
	{
		*pDim = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
	}
	else if (m_flags & kImageFlag3D)
	{
		*pDim = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
	}
	else
	{
		*pDim = D3D11_RESOURCE_DIMENSION_BUFFER;
	}
}

void CBufferResource::GetDesc(D3D11_BUFFER_DESC* pDesc)
{
	pDesc->ByteWidth = (UINT)GetSize();
	pDesc->Usage = D3D11_USAGE_DEFAULT;
	pDesc->BindFlags = 0;
	pDesc->CPUAccessFlags = 0;
	pDesc->MiscFlags = 0;
	pDesc->StructureByteStride = 0;
	if (m_flags & kResourceFlagShaderReadable)
	{
		pDesc->BindFlags |= D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_CONSTANT_BUFFER;
	}
	if (m_flags & kResourceFlagShaderWritable)
	{
		pDesc->BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}
	if (m_flags & kBufferFlagVertices)
	{
		pDesc->BindFlags |= D3D11_BIND_VERTEX_BUFFER;
	}
	if (m_flags & kBufferFlagIndices)
	{
		pDesc->BindFlags |= D3D11_BIND_INDEX_BUFFER;
	}
}

void CImageResource::GetDesc(D3D11_TEXTURE1D_DESC* pDesc)
{
	VK_ASSERT((m_flags & kImageFlag1D) != 0 && "Invalid cast to 1D texture");

	pDesc->Width = GetWidth();
	pDesc->MipLevels = GetMipCount();
	pDesc->ArraySize = GetSliceCount();
	pDesc->Format = ConvertFormat(GetFormat());
	pDesc->Usage = D3D11_USAGE_DEFAULT;
	pDesc->BindFlags = 0;
	pDesc->CPUAccessFlags = 0;
	pDesc->MiscFlags = 0;
	if (m_flags & kImageFlagColorAttachment)
	{
		pDesc->BindFlags |= D3D11_BIND_RENDER_TARGET;
	}
	if (m_flags & (kImageFlagDepthAttachment | kImageFlagStencilAttachment))
	{
		pDesc->BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	}
	if (m_flags & kResourceFlagShaderReadable)
	{
		pDesc->BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}
	if (m_flags & kResourceFlagShaderWritable)
	{
		pDesc->BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}
}

void CImageResource::GetDesc(D3D11_TEXTURE2D_DESC* pDesc)
{
	VK_ASSERT((m_flags & kImageFlag2D) != 0 && "Invalid cast to 2D texture");

	pDesc->Width = GetWidth();
	pDesc->Height = GetHeight();
	pDesc->MipLevels = GetMipCount();
	pDesc->ArraySize = GetSliceCount();
	pDesc->Format = ConvertFormat(GetFormat());
	pDesc->Usage = D3D11_USAGE_DEFAULT;
	pDesc->SampleDesc.Count = 1;
	pDesc->SampleDesc.Quality = 0;
	pDesc->BindFlags = 0;
	pDesc->CPUAccessFlags = 0;
	pDesc->MiscFlags = (m_flags & kImageFlagCube) ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
	if (m_flags & kImageFlagColorAttachment)
	{
		pDesc->BindFlags |= D3D11_BIND_RENDER_TARGET;
	}
	if (m_flags & (kImageFlagDepthAttachment | kImageFlagStencilAttachment))
	{
		pDesc->BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	}
	if (m_flags & kResourceFlagShaderReadable)
	{
		pDesc->BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}
	if (m_flags & kResourceFlagShaderWritable)
	{
		pDesc->BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}
}

void CImageResource::GetDesc(D3D11_TEXTURE3D_DESC* pDesc)
{
	VK_ASSERT((m_flags & kImageFlag3D) != 0 && "Invalid cast to 3D texture");

	pDesc->Width = GetWidth();
	pDesc->Height = GetHeight();
	pDesc->Depth = GetDepth();
	pDesc->MipLevels = GetMipCount();
	pDesc->Format = ConvertFormat(GetFormat());
	pDesc->Usage = D3D11_USAGE_DEFAULT;
	pDesc->BindFlags = 0;
	pDesc->CPUAccessFlags = 0;
	pDesc->MiscFlags = 0;
	if (m_flags & kImageFlagColorAttachment)
	{
		pDesc->BindFlags |= D3D11_BIND_RENDER_TARGET;
	}
	if (m_flags & (kImageFlagDepthAttachment | kImageFlagStencilAttachment))
	{
		pDesc->BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	}
	if (m_flags & kResourceFlagShaderReadable)
	{
		pDesc->BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}
	if (m_flags & kResourceFlagShaderWritable)
	{
		pDesc->BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}
}

HRESULT CResourceView::GetResource(ID3D11Resource** ppResource)
{
	*ppResource = GetResource();
	(*ppResource)->AddRef();
	return S_OK;
}

void CImageView::GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc)
{
	CImageResource* const pImage = GetResource();
	const bool bMultiSampled = pImage->GetFlag(kImageFlagMultiSampled);
	VK_ASSERT(m_numMips == 1 && "Invalid view for RTV description");
	
	pDesc->Format = ConvertFormat(m_format);
	switch (m_type)
	{
	case VK_IMAGE_VIEW_TYPE_1D:
		pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
		pDesc->Texture1D.MipSlice = m_firstMip;
		break;
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
		pDesc->Texture1DArray.MipSlice = m_firstMip;
		pDesc->Texture1DArray.FirstArraySlice = m_firstSlice;
		pDesc->Texture1DArray.ArraySize = m_numSlices;
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
		if (bMultiSampled)
		{
			pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			pDesc->Texture2D.MipSlice = m_firstMip;
		}
		break;
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		if (bMultiSampled)
		{
			pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
			pDesc->Texture2DMSArray.FirstArraySlice = m_firstSlice;
			pDesc->Texture2DMSArray.ArraySize = m_numSlices;
		}
		else
		{
			pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			pDesc->Texture2DArray.MipSlice = m_firstMip;
			pDesc->Texture2DArray.FirstArraySlice = m_firstSlice;
			pDesc->Texture2DArray.ArraySize = m_numSlices;
		}
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		pDesc->Texture3D.MipSlice = m_firstMip;
		pDesc->Texture3D.FirstWSlice = m_firstSlice;
		pDesc->Texture3D.WSize = m_numSlices;
		break;
	default:
		VK_ASSERT(false && "Unsupported RTV description type");
		break;
	}
}

}
