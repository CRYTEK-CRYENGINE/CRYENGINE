// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKResourceView.hpp"
#include "VKDevice.hpp"

namespace NCryVulkan
{

static void MapComponents(VkImageViewCreateInfo& info, EImageSwizzle swizzle)
{
	switch (swizzle)
	{
	default:
		VK_ASSERT(false && "Unknown swizzle, using identity");
	// Fall through
	case kImageSwizzleRGBA:
		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		break;
	case kImageSwizzleBGRA:
		info.components.r = VK_COMPONENT_SWIZZLE_B;
		info.components.g = VK_COMPONENT_SWIZZLE_G;
		info.components.b = VK_COMPONENT_SWIZZLE_R;
		info.components.a = VK_COMPONENT_SWIZZLE_A;
		break;
	case kImageSwizzleBGR1:
		info.components.r = VK_COMPONENT_SWIZZLE_B;
		info.components.g = VK_COMPONENT_SWIZZLE_G;
		info.components.b = VK_COMPONENT_SWIZZLE_R;
		info.components.a = VK_COMPONENT_SWIZZLE_ONE;
		break;
	case kImageSwizzle000A:
		info.components.r = VK_COMPONENT_SWIZZLE_ZERO;
		info.components.g = VK_COMPONENT_SWIZZLE_ZERO;
		info.components.b = VK_COMPONENT_SWIZZLE_ZERO;
		info.components.a = VK_COMPONENT_SWIZZLE_R;
		break;
	case kImageSwizzleDD00:
	case kImageSwizzleSS00:
		info.components.r = VK_COMPONENT_SWIZZLE_R;
		info.components.g = VK_COMPONENT_SWIZZLE_R;
		info.components.b = VK_COMPONENT_SWIZZLE_ZERO;
		info.components.a = VK_COMPONENT_SWIZZLE_ZERO;
		break;
	}
}

CBufferView::CBufferView(CBufferResource* pBuffer, uint32_t offset, uint32_t bytes, VkFormat format)
	: CResourceView(pBuffer)
	, m_offset(offset)
	, m_bytes(bytes)
	, m_texelFormat(format)
{
	VK_ASSERT(bytes != 0 && "Attempt to create empty view");
	VK_ASSERT(offset % pBuffer->GetStride() == 0 && "Buffer access not aligned to stride");
	VK_ASSERT(offset + bytes <= pBuffer->GetStride() * pBuffer->GetElementCount() && "Buffer access out of bounds");

	if (!IsStructured())
	{
		VkBufferViewCreateInfo info;
		info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.buffer = GetResource()->GetHandle();
		info.format = m_texelFormat;
		info.offset = m_offset;
		info.range = m_bytes;
		const VkResult result = vkCreateBufferView(GetDevice()->GetVkDevice(), &info, nullptr, &m_texelView);
		VK_ASSERT(result == VK_SUCCESS && "Invalid format specified for texel-buffer-view");
	}
}

CBufferView::~CBufferView()
{
	m_texelView.Destroy(vkDestroyBufferView, GetDevice()->GetVkDevice());
}

void CBufferView::Destroy()
{
	GetDevice()->DeferDestruction(std::move(*this));
	CRefCounted::Destroy();
}

CImageView::CImageView(CImageResource* pImage, VkFormat format, VkImageViewType viewType, uint32_t firstMip, uint32_t numMips, uint32_t firstSlice, uint32_t numSlices, EImageSwizzle swizzle)
	: CResourceView(pImage)
{
	const uint32_t actualNumMips = (numMips == VK_REMAINING_MIP_LEVELS) ? pImage->GetMipCount() - firstMip : numMips;
	const uint32_t actualNumSlices = (numSlices == VK_REMAINING_ARRAY_LAYERS) ? pImage->GetSliceCount() - firstSlice : numSlices;
	if (viewType == VK_IMAGE_VIEW_TYPE_RANGE_SIZE) // Handle "default" view
	{
		switch (pImage->GetDimensionality())
		{
		case VK_IMAGE_TYPE_1D:
			viewType = actualNumSlices == 1 ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
			break;
		case VK_IMAGE_TYPE_2D:
			viewType = pImage->GetFlag(kImageFlagCube)
				? (actualNumSlices <= 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
				: (actualNumSlices == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
			break;
		case VK_IMAGE_TYPE_3D:
			viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		}
	}

	const bool bIsCubeLike = (viewType == VK_IMAGE_VIEW_TYPE_CUBE) || (viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);
	const bool bIsArrayLike = (viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY) || (viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY) || (viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);	
	VK_ASSERT(firstMip + actualNumMips <= pImage->GetMipCount() && "Image mips out of range");
	VK_ASSERT(firstSlice + actualNumSlices <= pImage->GetSliceCount() && "Image slices out of range");
	VK_ASSERT(actualNumMips * actualNumSlices >= 1 && "No subresources selected");
	VK_ASSERT((!bIsCubeLike || (actualNumSlices % 6 == 0)) && "Cannot create cubes when number of slices is not a multiple of 6");
	VK_ASSERT((bIsArrayLike || (actualNumSlices == (bIsCubeLike ? 6 : 1))) && "Cannot create non-array view when slice count is not 1");
	VK_ASSERT((viewType != VK_IMAGE_VIEW_TYPE_3D || actualNumSlices == 1) && "Cannot create array view on volume textures");

	VkImageViewCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.image = pImage->GetHandle();
	info.viewType = viewType;
	info.format = format == VK_FORMAT_UNDEFINED ? pImage->GetFormat() : format;
	info.subresourceRange.baseMipLevel = firstMip;
	info.subresourceRange.levelCount = actualNumMips;
	info.subresourceRange.baseArrayLayer = firstSlice;
	info.subresourceRange.layerCount = actualNumSlices;
	MapComponents(info, swizzle);

	// Find aspect mask
	{
		bool bHasDepth = false;
		bool bHasStencil = false;
		switch (pImage->GetFormat())
		{
		case VK_FORMAT_S8_UINT:
			bHasDepth = !bHasDepth;
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
			bHasStencil = true;
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D32_SFLOAT:
			bHasDepth = !bHasDepth;

			bHasDepth &= swizzle != kImageSwizzleSS00;
			bHasStencil &= swizzle != kImageSwizzleDD00;
			VK_ASSERT((bHasDepth || bHasStencil) && "Invalid swizzle for depth/stencil format");
			
			info.format = pImage->GetFormat(); // Views can never re-interpret depth/stencil formats
			info.subresourceRange.aspectMask = bHasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			info.subresourceRange.aspectMask |= bHasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
			break;
		default:
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}

	const VkResult result = vkCreateImageView(GetDevice()->GetVkDevice(), &info, nullptr, &m_imageView);
	VK_ASSERT(result == VK_SUCCESS && "Failed to create image view, check parameters");

	// These are only needed for GetDesc() / debugging. TODO: Remove?
	m_firstMip = firstMip;
	m_numMips = actualNumMips;
	m_firstSlice = firstSlice;
	m_numSlices = actualNumSlices;
	m_format = info.format;
	m_type = viewType;
}

CImageView::~CImageView()
{
	m_imageView.Destroy(vkDestroyImageView, GetDevice()->GetVkDevice());
}

void CImageView::Destroy()
{
	// swap chain views need to be released immediately
	if (!GetResource()->GetFlag(kImageFlagSwapchain))
	{
		GetDevice()->DeferDestruction(std::move(*this));
	}

	CRefCounted::Destroy();
}

}
