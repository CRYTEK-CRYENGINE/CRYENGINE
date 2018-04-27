// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKImageResource.hpp"
#include "VKDevice.hpp"

namespace NCryVulkan
{

CImageResource::~CImageResource()
{
	if (m_flags & kImageFlagSwapchain)
	{
		m_hVkImageResource.Detach(); // We don't own this object
	}
	else
	{
		m_hVkImageResource.Destroy(vkDestroyImage, GetDevice()->GetVkDevice());
	}
}

VkResult CImageResource::InitFromSwapchain(VkImage image, VkImageLayout layout, uint32_t width, uint32_t height, VkFormat format)
{
	VK_ASSERT(!m_hVkImageResource && "Image already initialized");

	m_hVkImageResource = image;
	m_eCurrentLayout = layout;
	m_VkCreateInfo.format = format;
	m_VkCreateInfo.extent.width = width;
	m_VkCreateInfo.extent.height = height;
	m_VkCreateInfo.extent.depth = 1;
	m_VkCreateInfo.arrayLayers = 1;
	m_VkCreateInfo.mipLevels = 1;
	m_flags = kImageFlagSwapchain | kImageFlag2D | kImageFlagColorAttachment; // TODO: Should check ACTUAL flags passed into swapchain here

	return VK_SUCCESS;
}

VkResult CImageResource::Init(VkImage image, const VkImageCreateInfo& createInfo)
{
	VK_ASSERT(!m_hVkImageResource && "Image already initialized");
	m_hVkImageResource = image;
	m_VkCreateInfo = createInfo;

	VK_ASSERT(~createInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT && "Sparse storage not implemented");
	VK_ASSERT(createInfo.samples == VK_SAMPLE_COUNT_1_BIT && "Multi-sampled images not implemented");
	VK_ASSERT(~createInfo.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT && "Transient storage not implemented");
	VK_ASSERT(createInfo.sharingMode == VK_SHARING_MODE_EXCLUSIVE && "Sharing not implemented");

	{
		m_eCurrentLayout = createInfo.initialLayout;

		// Analyze flags
		m_flags = kResourceFlagNone;
		if (createInfo.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
		{
			VK_ASSERT((createInfo.imageType == VK_IMAGE_TYPE_2D) && createInfo.arrayLayers >= 6 && "Bad dimensions for cube(-array)");
			m_flags |= kImageFlagCube;
		}
		if (createInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT)
		{
			m_flags |= kResourceFlagShaderWritable;
		}
		if (createInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT)
		{
			m_flags |= kResourceFlagShaderReadable;
		}
		if (createInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			m_flags |= kImageFlagColorAttachment;
		}
		if (createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			switch (createInfo.format)
			{
			case VK_FORMAT_S8_UINT:
			case VK_FORMAT_D16_UNORM_S8_UINT:
			case VK_FORMAT_D24_UNORM_S8_UINT:
			case VK_FORMAT_D32_SFLOAT_S8_UINT:
				m_flags |= kImageFlagStencilAttachment;
			// Fall through
			case VK_FORMAT_D16_UNORM:
			case VK_FORMAT_X8_D24_UNORM_PACK32:
			case VK_FORMAT_D32_SFLOAT:
				if (createInfo.format != VK_FORMAT_S8_UINT)
				{
					m_flags |= kImageFlagDepthAttachment;
				}
				break;
			}
			VK_ASSERT((m_flags & (kImageFlagDepthAttachment | kImageFlagStencilAttachment)) && "Invalid format for usage with D/S attachment");
		}
		if (createInfo.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		{
			m_flags |= kResourceFlagCopyReadable;
		}
		if (createInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		{
			m_flags |= kResourceFlagCopyWritable;
		}
		if (createInfo.tiling == VK_IMAGE_TILING_LINEAR)
		{
			m_flags |= kResourceFlagLinear;
		}
		if (createInfo.samples != VK_SAMPLE_COUNT_1_BIT)
		{
			m_flags |= kImageFlagMultiSampled;
		}
		switch (createInfo.imageType)
		{
		case VK_IMAGE_TYPE_1D:
			m_flags |= kImageFlag1D;
			break;
		case VK_IMAGE_TYPE_2D:
			m_flags |= kImageFlag2D;
			break;
		case VK_IMAGE_TYPE_3D:
			m_flags |= kImageFlag3D;
			break;
		default:
			VK_ASSERT(false && "Unknown image type");
			break;
		}
	}

	return VK_SUCCESS;
}

VkResult CImageResource::InitAsNull(VkImageType dimensionality)
{
	VkImageCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = (dimensionality == VK_IMAGE_TYPE_2D) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	info.imageType = dimensionality;
	info.format = VK_FORMAT_R8G8B8A8_UNORM;
	info.extent.width = 1;
	info.extent.height = 1;
	info.extent.depth = 1;
	info.mipLevels = 1;
	info.arrayLayers = (dimensionality == VK_IMAGE_TYPE_2D) ? 6 : 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkResult result = Init(VK_NULL_HANDLE, info);
	CMemoryResource::Init(kHeapSources, CMemoryHandle());

	m_flags |= kResourceFlagNull;
	return result;
}

CAutoHandle<VkImageView> CImageResource::CreateView(VkImageViewType viewType, VkFormat format, const VkComponentMapping& componentSwizzle, const VkImageSubresourceRange& range)
{
	switch (viewType)
	{
	case VK_IMAGE_VIEW_TYPE_1D:
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		VK_ASSERT((m_flags & kImageFlag1D) != 0 && "Cannot create 1D view on non-1D image");
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		VK_ASSERT((m_flags & kImageFlag2D) != 0 && "Cannot create 2D view on non-2D image");
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		VK_ASSERT((m_flags & kImageFlag3D) != 0 && "Cannot create 3D view on non-3D image");
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		VK_ASSERT((m_flags & kImageFlag2D) != 0 && (m_flags & kImageFlagCube) != 0 && "Cannot create Cube view on non-cube image");
		break;
	default:
		VK_ASSERT(false && "Unknown view type");
	}
	switch (viewType)
	{
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		VK_ASSERT(range.layerCount % 6 == 0 && "Array view is not a multiple of 6 (required for cube array)");
	// Fall through
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		VK_ASSERT(range.baseArrayLayer < GetSliceCount() && range.baseArrayLayer + range.layerCount <= GetSliceCount() && "Array view out of range");
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
		VK_ASSERT(range.baseArrayLayer + 6 <= GetSliceCount() && range.layerCount == 6 && "Array view is 6 slices (required for single cube)");
		break;
	default:
		VK_ASSERT(range.baseArrayLayer < GetSliceCount() && range.layerCount == 1 && "Non-array view must contain exactly one slice");
		break;
	}
	VK_ASSERT(range.baseMipLevel < GetMipCount() && range.baseMipLevel + range.levelCount <= GetMipCount() && "Mip slice does not fit inside texture");
	if (range.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
	{
		const EResourceFlag depthFlag = (range.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) ? kImageFlagDepthAttachment : kResourceFlagNone;
		const EResourceFlag stencilFlag = (range.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) ? kImageFlagStencilAttachment : kResourceFlagNone;
		VK_ASSERT((m_flags & (depthFlag | stencilFlag)) == (depthFlag | stencilFlag) && "Cannot create D/S-view on non-D/S-image");
	}
	else
	{
		VK_ASSERT(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT && "No aspects selected");
		VK_ASSERT((m_flags & (kImageFlagDepthAttachment | kImageFlagStencilAttachment)) == 0 && "Cannot create color-view on D/S-image");
	}

	VkImageViewCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.image = m_hVkImageResource;
	info.viewType = viewType;
	info.format = format;
	info.components = componentSwizzle; // TODO: Handle A8 specially here?
	info.subresourceRange = range;

	CAutoHandle<VkImageView> view;
	const VkResult result = vkCreateImageView(GetDevice()->GetVkDevice(), &info, nullptr, &view);
	if (result != VK_SUCCESS)
	{
		VK_ERROR("Image view creation failed (return value %d)", result);
	}
	return view;
}

void CImageResource::Destroy()
{

	auto& Scheduler = GetDevice()->GetScheduler();
	FVAL64 fenceValues[CMDQUEUE_NUM];

	// Create a "now" timestamp, in the absence of precise usage-timestamps
	fenceValues[CMDQUEUE_GRAPHICS] = Scheduler.InsertFence(CMDQUEUE_GRAPHICS);
	fenceValues[CMDQUEUE_COMPUTE ] = Scheduler.InsertFence(CMDQUEUE_COMPUTE);
	fenceValues[CMDQUEUE_COPY    ] = Scheduler.InsertFence(CMDQUEUE_COPY);

	GetDevice()->ReleaseLater(fenceValues, this, GetFlag(kResourceFlagReusable));
}

//---------------------------------------------------------------------------------------------------------------------
const char* AccessToString(VkAccessFlags accessFlags);
const char* LayoutToString(VkImageLayout layout)
{
	static int buf = 0;
	static char str[2][512], *ret;

	ret = str[buf ^= 1];
	*ret = '\0';

	if (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		strcat(ret, " Common/Present");
		return ret;
	}

	else if (layout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		strcat(ret, " Undefined");
	}
	else if (layout == VK_IMAGE_LAYOUT_GENERAL)
	{
		strcat(ret, " General");
	}
	else if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		strcat(ret, " RT");
	}
	else if (layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		strcat(ret, " DepthW");
	}
	else if (layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
	{
		strcat(ret, " DepthR");
	}
	else if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		strcat(ret, " Shader Read");
	}
	else if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		strcat(ret, " CopyS");
	}
	else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		strcat(ret, " CopyD");
	}
	else if (layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
	{
		strcat(ret, " PreInit");
	}

	return ret;
}

VkImageLayout CImageResource::TransitionBarrier(CCommandList* pCmdList, VkImageLayout eDesiredLayout)
{
	VkImageLayout ePreviousLayout = m_eCurrentLayout;

	if ((m_eCurrentLayout != eDesiredLayout) || IsIncompatibleAccess(m_eCurrentAccess, m_eAnnouncedAccess))
	{
		// Determine image aspects (TODO: should be more correct)
		VkImageAspectFlags aspects = VK_IMAGE_ASPECT_COLOR_BIT;
		if (GetFlag(kImageFlagDepthAttachment))
		{
			aspects = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (GetFlag(kImageFlagStencilAttachment))
			{
				aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}

		VkImageMemoryBarrier barrierDesc;

		barrierDesc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrierDesc.pNext = nullptr;
		barrierDesc.srcAccessMask = m_eCurrentAccess;
		barrierDesc.dstAccessMask = m_eAnnouncedAccess;
		barrierDesc.oldLayout = ePreviousLayout;
		barrierDesc.newLayout = eDesiredLayout;
		barrierDesc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // pCmdList->GetVkQueueFamily/Index
		barrierDesc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // pCmdList->GetVkQueueFamily/Index
		barrierDesc.image = m_hVkImageResource;
		barrierDesc.subresourceRange.aspectMask = aspects;
		barrierDesc.subresourceRange.baseMipLevel = 0;
		barrierDesc.subresourceRange.levelCount = GetMipCount();
		barrierDesc.subresourceRange.baseArrayLayer = 0;
		barrierDesc.subresourceRange.layerCount = GetSliceCount();

		VK_LOG(VK_BARRIER_ANALYZER, "Image resource barrier change %s (Vk: %p): %s ->%s && %s ->%s", GetName().c_str(), barrierDesc.image, LayoutToString(m_eCurrentLayout), LayoutToString(eDesiredLayout), AccessToString(m_eCurrentAccess), AccessToString(m_eAnnouncedAccess));

		pCmdList->ResourceBarrier(1, &barrierDesc);

		m_eCurrentAccess = m_eAnnouncedAccess;
		m_eAnnouncedAccess = 0;
		m_eCurrentLayout = eDesiredLayout;
	}

	return ePreviousLayout;
}

VkImageLayout CImageResource::TransitionBarrier(CCommandList* pCmdList, VkImageLayout layout, const CImageView& view)
{
	VK_NOT_IMPLEMENTED;
	return m_eCurrentLayout;
}

}
