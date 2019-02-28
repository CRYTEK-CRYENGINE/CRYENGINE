// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VKBase.hpp"
#include "VKMemoryResource.hpp"
#include "VKBufferResource.hpp"
#include "VKImageResource.hpp"

namespace NCryVulkan
{

// Determines how the channels in an image are swizzled when accessed by the shader / as pipeline output.
enum EImageSwizzle
{
	kImageSwizzleRGBA, // RGBA -> RGBA, identity mapping
	kImageSwizzleBGRA, // RGBA -> BGRA, use for BGRA
	kImageSwizzleBGR1, // RGBA -> BGR1, use for BGRX
	kImageSwizzle000A, // RGBA -> 000R, use for A8
	kImageSwizzleDD00, // RGBA -> RR00, use for depth access
	kImageSwizzleSS00, // RGBA -> RR00, use for stencil access
};

class CResourceView : public CDeviceObject
{
public:
	CResourceView(CMemoryResource* pResource) : CDeviceObject(pResource->GetDevice()), m_pResource(pResource) {}
	CResourceView(CResourceView&& other) = default;
	CResourceView&   operator=(CResourceView&& other) = default;

	CMemoryResource* GetResource() const { return m_pResource; }

	HRESULT          GetResource(ID3D11Resource** ppResource); // Legacy, to be removed!

private:
	_smart_ptr<CMemoryResource> m_pResource;
};

class CBufferView : public CResourceView
{
public:
	using CResourceView::GetResource; // Legacy, to be removed!

	CBufferView(CBufferResource* pBuffer, uint32_t offset, uint32_t bytes, VkFormat format = VK_FORMAT_UNDEFINED);
	CBufferView(CBufferView&& other) = default;
	CBufferView& operator=(CBufferView&& other) = default;
	virtual ~CBufferView() override;

	CBufferResource* GetResource() const  { return static_cast<CBufferResource*>(CResourceView::GetResource()); }
	VkBufferView     GetHandle() const    { return m_texelView; }
	bool             IsStructured() const { return m_texelFormat == VK_FORMAT_UNDEFINED; }

	// For non-texel buffer (ie, cannot be used for VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER and VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER).
	// The view must have been created with the VK_FORMAT_UNDEFINED format.
	void FillInfo(VkDescriptorBufferInfo& info) const
	{
		info.buffer = GetResource()->GetHandle();
		info.offset = m_offset;
		info.range = m_bytes;
	}

	// For texel-buffer (ie, can be used for VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER and VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER).
	// The view must have been created with a valid format.
	void FillInfo(VkBufferView& info)
	{
		VK_ASSERT(!IsStructured() && "Structured buffers must use the other FillInfo overload");
		info = m_texelView;
	}

private:
	virtual void Destroy() override; // Uses CDevice::DeferDestruction

	uint32_t                  m_offset;
	uint32_t                  m_bytes;
	VkFormat                  m_texelFormat; // Only for non-structured-buffers
	CAutoHandle<VkBufferView> m_texelView;   // Only for non-structured-buffers
};

class CImageView : public CResourceView
{
public:
	using CResourceView::GetResource; // Legacy, to be removed!

	CImageView(CImageResource* pImage,
	           VkFormat format = VK_FORMAT_UNDEFINED,                                   // VK_FORMAT_UNDEFINED: Use format specified when image was created.
	           VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_RANGE_SIZE,                // VK_IMAGE_VIEW_TYPE_RANGE_SIZE: Pick automatic based on image type
	           uint32_t firstMip = 0, uint32_t numMips = VK_REMAINING_MIP_LEVELS,       // Mip range
	           uint32_t firstSlice = 0, uint32_t numSlices = VK_REMAINING_ARRAY_LAYERS, // Array range
	           EImageSwizzle swizzle = kImageSwizzleRGBA);                              // Swizzle for exotic formats
	CImageView(CImageView&& other) = default;
	CImageView& operator=(CImageView&& other) = default;
	virtual ~CImageView() override;

	CImageResource* GetResource() const   { return static_cast<CImageResource*>(CResourceView::GetResource()); }
	VkImageView     GetHandle() const     { return m_imageView; }
	uint32_t        GetFirstMip() const   { return m_firstMip; }
	uint32_t        GetMipCount() const   { return m_numMips; }
	uint32_t        GetFirstSlice() const { return m_firstSlice; }
	uint32_t        GetSliceCount() const { return m_numSlices; }

	// For non-fused types (ie, cannot be used for VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	void FillInfo(VkDescriptorImageInfo& info, VkImageLayout layout) const
	{
		info.sampler = VK_NULL_HANDLE;
		info.imageView = m_imageView;
		info.imageLayout = layout;
	}

	// For fused types (ie, can be used for VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	void FillInfo(VkDescriptorImageInfo& info, VkImageLayout layout, const CSampler& sampler) const
	{
		info.sampler = sampler.GetHandle();
		info.imageView = m_imageView;
		info.imageLayout = layout;
	}

	VkImageSubresourceRange GetSubresourceRange(VkImageAspectFlags aspectMask) const
	{
		VkImageSubresourceRange result;
		result.aspectMask = aspectMask;
		result.baseMipLevel = m_firstMip;
		result.levelCount = m_numMips;
		result.baseArrayLayer = m_firstSlice;
		result.layerCount = m_numSlices;
		return result;
	}

	void GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc); // Legacy, to be removed!

private:
	virtual void Destroy() override; // Uses CDevice::DeferDestruction

	CAutoHandle<VkImageView> m_imageView;
	uint32_t                 m_firstMip;
	uint32_t                 m_numMips;
	uint32_t                 m_firstSlice;
	uint32_t                 m_numSlices;
	VkFormat                 m_format;
	VkImageViewType          m_type;
};

}
