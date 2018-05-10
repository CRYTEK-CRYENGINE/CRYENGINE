// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "VKBase.hpp"
#include "VKMemoryResource.hpp"

namespace NCryVulkan
{

class CDevice;
class CSwapChain;
class CCommandList;
class CImageView;

class CImageResource final : public CMemoryResource
{
public:
	friend class CDevice;
	friend class CSwapChain;

	CImageResource(CDevice* pDevice) : CMemoryResource(pDevice) {}
	virtual ~CImageResource() override;

	CImageResource(CImageResource&&) = default;
	CImageResource& operator=(CImageResource&&) = default;

	VkResult                 InitFromSwapchain(VkImage image, VkImageLayout layout, uint32_t width, uint32_t height, VkFormat format);
	VkResult                 Init(VkImage image, const VkImageCreateInfo& createInfo);
	VkResult                 InitAsNull(VkImageType dimensionality);
	CAutoHandle<VkImageView> CreateView(VkImageViewType viewType, VkFormat format, const VkComponentMapping& componentSwizzle, const VkImageSubresourceRange& range);

	VkImage                  GetHandle() const         { return m_hVkImageResource; }
	const VkImageCreateInfo& GetCreateInfo() const     { return m_VkCreateInfo;  }

	VkImageLayout            GetLayout() const         { return m_eCurrentLayout; }
	VkImageType              GetDimensionality() const { return (m_flags & kImageFlag2D) ? VK_IMAGE_TYPE_2D : (m_flags & kImageFlag3D) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_1D; }
	VkFormat                 GetFormat() const         { return m_VkCreateInfo.format; }
	uint32_t                 GetWidth() const          { return m_VkCreateInfo.extent.width; }
	uint32_t                 GetHeight() const         { return m_VkCreateInfo.extent.height; }
	uint32_t                 GetDepth() const          { return m_VkCreateInfo.extent.depth; }
	uint32_t                 GetSliceCount() const     { return m_VkCreateInfo.arrayLayers; }
	uint32_t                 GetMipCount() const       { return m_VkCreateInfo.mipLevels; }

	void                     GetDesc(D3D11_TEXTURE1D_DESC*); // Legacy, to be removed!
	void                     GetDesc(D3D11_TEXTURE2D_DESC*); // Legacy, to be removed!
	void                     GetDesc(D3D11_TEXTURE3D_DESC*); // Legacy, to be removed!

	VkImageLayout            TransitionBarrier(CCommandList* pCmdList, VkImageLayout eDesiredLayout);
	VkImageLayout            TransitionBarrier(CCommandList* pCmdList, VkImageLayout eDesiredLayout, const CImageView& view);

	typedef VkImageCreateInfo VkCreateInfo;
	typedef VkImage VkResource;

private:
	virtual void Destroy() override; // Handles usage-tracked RefCount of 0

	CAutoHandle<VkImage> m_hVkImageResource;

	VkImageLayout        m_eCurrentLayout;

	VkImageCreateInfo    m_VkCreateInfo;
};

static_assert(std::is_nothrow_move_constructible<CImageResource>::value, "CImageResource must be movable");

}
