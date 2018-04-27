// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKBufferResource.hpp"
#include "VKDevice.hpp"

namespace NCryVulkan
{

CBufferResource::~CBufferResource()
{
	m_hVkBufferResource.Destroy(vkDestroyBuffer, GetDevice()->GetVkDevice());
}

VkResult CBufferResource::Init(VkBuffer buffer, const VkBufferCreateInfo& createInfo)
{
	VK_ASSERT(!m_hVkBufferResource && "Buffer already initialized");
	m_hVkBufferResource = buffer;
	m_VkCreateInfo = createInfo;

	VK_ASSERT(createInfo.sharingMode == VK_SHARING_MODE_EXCLUSIVE && "Sharing not implemented");
	VK_ASSERT(~createInfo.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT && "Sparse storage not implemented");

	{
		// Analyze flags
		m_flags = kResourceFlagLinear;

		if (createInfo.usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT))
		{
			m_flags |= kResourceFlagShaderReadable;
		}
		if (createInfo.usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
		{
			m_flags |= kResourceFlagShaderWritable;
		}
		if (createInfo.usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
		{
			m_flags |= kBufferFlagTexels;
		}
		if (createInfo.usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
		{
			m_flags |= kBufferFlagIndices;
		}
		if (createInfo.usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
		{
			m_flags |= kBufferFlagVertices;
		}
		if (createInfo.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
		{
			m_flags |= kResourceFlagCopyReadable;
		}
		if (createInfo.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		{
			m_flags |= kResourceFlagCopyWritable;
		}
	}

	return VK_SUCCESS;
}

VkResult CBufferResource::InitAsNull()
{
	VkBufferCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.size = 16;
	info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;

	VkResult result = Init(VK_NULL_HANDLE, info);
	CMemoryResource::Init(kHeapSources, CMemoryHandle());
	SetStrideAndElementCount(16, 1);

	m_flags |= kResourceFlagNull;
	m_stride = 16;
	m_elements = 1;

	return result;
}

CAutoHandle<VkBufferView> CBufferResource::CreateView(VkFormat format, VkDeviceSize offset, VkDeviceSize bytes)
{
	VK_ASSERT(m_hVkBufferResource && (m_flags & kBufferFlagTexels) && "Cannot create buffer view on a non-texel buffer");

	VkBufferViewCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.buffer = m_hVkBufferResource;
	info.format = format;
	info.offset = offset;
	info.range = bytes;

	CAutoHandle<VkBufferView> view;
	const VkResult result = vkCreateBufferView(GetDevice()->GetVkDevice(), &info, nullptr, &view);
	if (result != VK_SUCCESS)
	{
		VK_ERROR("Buffer view creation failed (return value %d)", result);
	}
	return view;
}

void CBufferResource::Destroy()
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

uint64_t CBufferResource::RangeBarrier(CCommandList* pCmdList)
{
	if (!m_bLastAccessBarrierWasFull || IsIncompatibleAccess(m_eCurrentAccess, m_eAnnouncedAccess))
	{
		VkBufferMemoryBarrier barrierDesc;

		barrierDesc.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrierDesc.pNext = nullptr;
		barrierDesc.srcAccessMask = m_eCurrentAccess;
		barrierDesc.dstAccessMask = m_eAnnouncedAccess;
		barrierDesc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // pCmdList->GetVkQueueFamily/Index
		barrierDesc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // pCmdList->GetVkQueueFamily/Index
		barrierDesc.buffer = m_hVkBufferResource;
		barrierDesc.offset = 0;
		barrierDesc.size = GetElementCount() * GetStride();

		VK_LOG(VK_BARRIER_ANALYZER, "Buffer resource barrier change %s (Vk: %p): full (%d, %d) && %s ->%s", GetName().c_str(), barrierDesc.buffer, 0, GetElementCount() * GetStride(), AccessToString(m_eCurrentAccess), AccessToString(m_eAnnouncedAccess));

		pCmdList->ResourceBarrier(1, &barrierDesc);

		m_eCurrentAccess = m_eAnnouncedAccess;
		m_eAnnouncedAccess = 0;

		m_bLastAccessBarrierWasFull = true;
	}

	return 0;
}

uint64_t CBufferResource::RangeBarrier(CCommandList* pCmdList, uint32_t offset, uint32_t size)
{
	if (true /* substitute for not tracking the access-flags of all sub-ranges */ || IsIncompatibleAccess(m_eCurrentAccess, m_eAnnouncedAccess))
	{
		VkBufferMemoryBarrier barrierDesc;

		barrierDesc.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrierDesc.pNext = nullptr;
		barrierDesc.srcAccessMask = m_eCurrentAccess;
		barrierDesc.dstAccessMask = m_eAnnouncedAccess;
		barrierDesc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // pCmdList->GetVkQueueFamily/Index
		barrierDesc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // pCmdList->GetVkQueueFamily/Index
		barrierDesc.buffer = m_hVkBufferResource;
		barrierDesc.offset = offset;
		barrierDesc.size = size;

		VK_LOG(VK_BARRIER_ANALYZER, "Buffer resource barrier change %s (Vk: %p): partial (%d, %d) && %s ->%s", GetName().c_str(), barrierDesc.buffer, offset, size, AccessToString(m_eCurrentAccess), AccessToString(m_eAnnouncedAccess));

		pCmdList->ResourceBarrier(1, &barrierDesc);

		m_eCurrentAccess = m_eAnnouncedAccess;
		m_eAnnouncedAccess = 0;

		m_bLastAccessBarrierWasFull = false;
	}

	return 0;
}

//---------------------------------------------------------------------------------------------------------------------


CDynamicOffsetBufferResource::~CDynamicOffsetBufferResource()
{
	if (m_dynamicDescriptorSet != VK_NULL_HANDLE)
	{
		vkFreeDescriptorSets(GetDevice()->GetVkDevice(), GetDevice()->GetDescriptorPool(), 1, &m_dynamicDescriptorSet);
		m_dynamicDescriptorSet = VK_NULL_HANDLE;
	}
}

VkResult CDynamicOffsetBufferResource::Init(VkBuffer buffer, const VkBufferCreateInfo& createInfo)
{
	m_dynamicDescriptorSet = VK_NULL_HANDLE;
	VkResult result = CBufferResource::Init(buffer, createInfo);

	if (result == VK_SUCCESS)
	{
		m_flags |= kBufferFlagDynamicOffset;
	}

	return result;
}

VkResult CDynamicOffsetBufferResource::CreateDynamicDescriptorSet(VkDescriptorSetLayout layout, uint32_t bufferSize)
{
	CRY_ASSERT(layout != VK_NULL_HANDLE && m_hVkBufferResource);
	static_assert(CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS, "Only direct access constant buffers are guaranteed to have power-of-two size");

	CRY_ASSERT(bufferSize >= 256 && bufferSize <= 32768);
#ifndef _RELEASE
	if (!m_bufferSizeLog2) { m_bufferSizeLog2 =  uint32(IntegerLog2(NextPower2(bufferSize))); }
	CRY_ASSERT(              m_bufferSizeLog2 == uint32(IntegerLog2(NextPower2(bufferSize))));
#endif // !_RELEASE

	VkDescriptorSetAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = GetDevice()->GetDescriptorPool();
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	const VkResult result = vkAllocateDescriptorSets(GetDevice()->GetVkDevice(), &allocInfo, &m_dynamicDescriptorSet);
	if (result == VK_SUCCESS)
	{
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = m_hVkBufferResource;
		bufferInfo.offset = 0;
		bufferInfo.range = bufferSize;

		VkWriteDescriptorSet write;
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;
		write.dstSet = m_dynamicDescriptorSet;
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write.pImageInfo = nullptr;
		write.pBufferInfo = &bufferInfo;
		write.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(GetDevice()->GetVkDevice(), 1, &write, 0, nullptr);
	}

	return result;
}

}
