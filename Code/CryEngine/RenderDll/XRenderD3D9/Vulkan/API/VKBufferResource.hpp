// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VKBase.hpp"
#include "VKMemoryResource.hpp"

namespace NCryVulkan
{

class CDevice;
class CCommandList;
class CBufferView;

class CBufferResource : public CMemoryResource
{
public:
	friend class CDevice;

	CBufferResource(CDevice* pDevice) : CMemoryResource(pDevice) {}
	virtual ~CBufferResource() override;

	CBufferResource(CBufferResource&& r) = default;
	CBufferResource& operator=(CBufferResource&& r) = default;

	virtual VkResult          Init(VkBuffer buffer, const VkBufferCreateInfo& createInfo);
	VkResult                  InitAsNull();
	CAutoHandle<VkBufferView> CreateView(VkFormat format, VkDeviceSize offset, VkDeviceSize bytes);

	VkBuffer                  GetHandle() const       { return m_hVkBufferResource; }
	const VkBufferCreateInfo& GetCreateInfo() const   { return m_VkCreateInfo; }

	void                      SetStrideAndElementCount(uint32_t stride, uint32_t elementCount)
	{ 
		m_stride = stride;
		m_elements = elementCount;
		CRY_ASSERT(m_stride * m_elements > 0 && m_stride * m_elements <= m_VkCreateInfo.size);
	}

	uint32_t                  GetStride() const       { return m_stride; }
	uint32_t                  GetElementCount() const { return m_elements; }

	void                      GetDesc(D3D11_BUFFER_DESC* pDesc); // Legacy, to be removed

	uint64_t                  RangeBarrier(CCommandList* pCmdList);
	uint64_t                  RangeBarrier(CCommandList* pCmdList, uint32_t offset, uint32_t size);

	typedef VkBufferCreateInfo VkCreateInfo;
	typedef VkBuffer VkResource;

protected:
	bool m_bLastAccessBarrierWasFull = true;

	virtual void Destroy() override; // Handles usage-tracked RefCount of 0

	CAutoHandle<VkBuffer> m_hVkBufferResource;
	uint32_t              m_stride   = 1; // Size of element, in bytes
	uint32_t              m_elements = 0; // Number of elements

	VkBufferCreateInfo    m_VkCreateInfo;
};

static_assert(std::is_nothrow_move_constructible<CBufferResource>::value, "CBufferResource must be movable");


class CDynamicOffsetBufferResource final : public CBufferResource
{
public:
	CDynamicOffsetBufferResource(CDevice* pDevice) : CBufferResource(pDevice) {}
	virtual ~CDynamicOffsetBufferResource() override;

	virtual VkResult          Init(VkBuffer buffer, const VkBufferCreateInfo& createInfo) override;
	
	VkResult                  CreateDynamicDescriptorSet(VkDescriptorSetLayout layout, uint32_t bufferSize);
	VkDescriptorSet           GetDynamicDescriptorSet() const
	{
		return m_dynamicDescriptorSet;
	}

private:
#ifndef _RELEASE
	mutable uint32  m_bufferSizeLog2 = 0;
#endif // !_RELEASE
	VkDescriptorSet m_dynamicDescriptorSet; // Descriptor sets used for binding this buffer with dynamic offset
};

}
