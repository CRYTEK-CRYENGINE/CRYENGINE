// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../../DeviceManager/DeviceResourceSet.h"

using namespace NCryVulkan;

static NCryVulkan::CDevice* GetDevice()
{
	return GetDeviceObjectFactory().GetVKDevice();
}

static NCryVulkan::CCommandScheduler* GetScheduler()
{
	return GetDeviceObjectFactory().GetVKScheduler();
}

class CDeviceResourceSet_Vulkan : public CDeviceResourceSet
{
public:
	CDeviceResourceSet_Vulkan(CDevice* pDevice, CDeviceResourceSet::EFlags flags)
		: CDeviceResourceSet(flags)
		, m_pDevice(pDevice)
		, m_descriptorSetLayout(VK_NULL_HANDLE)
		, m_pDescriptorSet(nullptr)
	{}

	~CDeviceResourceSet_Vulkan();

	virtual bool UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags);

	VkDescriptorSetLayout GetVkDescriptorSetLayout() const { VK_ASSERT(m_descriptorSetLayout != VK_NULL_HANDLE && "Set not built"); return m_descriptorSetLayout; }
	VkDescriptorSet       GetVKDescriptorSet()       const { return m_descriptorSetHandle; }

	static VkDescriptorSetLayout CreateLayout(const VectorMap<SResourceBindPoint, SResourceBinding>& resources);


	// Calls specified functor with signature "void (CMemoryResource* pResource, bool bWritable)" for all resources used by the resource-set.
	template<typename TFunctor>
	void EnumerateInUseResources(const TFunctor& functor) const
	{
		for (const std::pair<_smart_ptr<CMemoryResource>, bool>& pair : m_inUseResources)
		{
			CMemoryResource* const pResource = pair.first.get();
			const bool bWritable = pair.second;
			functor(pResource, bWritable);
		}
	}

protected:

	bool FillDescriptors(const CDeviceResourceSetDesc& desc, const SDescriptorSet* descriptorSet);
	void ReleaseDescriptors();

	static VkDescriptorType GetDescriptorType(SResourceBindPoint bindPoint)
	{
		switch (bindPoint.slotType)
		{
		case SResourceBindPoint::ESlotType::ConstantBuffer:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		case SResourceBindPoint::ESlotType::Sampler:
			return VK_DESCRIPTOR_TYPE_SAMPLER;

		case SResourceBindPoint::ESlotType::TextureAndBuffer:
		{
			if (uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsTexture))
			{
				return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			}
			else
			{
				return uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsStructured)
					? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER        // StructuredBuffer<*> : register(t*)
					: VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER; // Buffer<*> : register(t*)
			}
		}

		case SResourceBindPoint::ESlotType::UnorderedAccessView:
		{
			if (uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsTexture))
			{
				return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			}
			else
			{
				return uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsStructured)
					? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER        // RWStructuredBuffer<*> : register(u*)
					: VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER; // RWBuffer<*> : register(u*)
			}
		}
		}

		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}

	static bool IsTexelBuffer(VkDescriptorType descriptorType)
	{
		return descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER || descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	}

	CDevice* const                                    m_pDevice;
	CAutoHandle<VkDescriptorSetLayout>                m_descriptorSetLayout;
	VkDescriptorSet                                   m_descriptorSetHandle;


	std::vector<std::pair<_smart_ptr<CMemoryResource>, bool>> m_inUseResources; // Required to keep resources alive while in resource-set.

	SDescriptorSet*      m_pDescriptorSet;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceResourceLayout_Vulkan : public CDeviceResourceLayout
{
public:
	CDeviceResourceLayout_Vulkan(CDevice* pDevice, UsedBindSlotSet requiredResourceBindings)
		: CDeviceResourceLayout(requiredResourceBindings)
		, m_pDevice(pDevice)
		, m_pipelineLayout(VK_NULL_HANDLE)
		, m_hash(0)
	{}

	~CDeviceResourceLayout_Vulkan();

	bool                    Init(const SDeviceResourceLayoutDesc& desc);

	const VkPipelineLayout&   GetVkPipelineLayout() const { return m_pipelineLayout; }
	uint64                    GetHash()             const { return m_hash; }
	const std::vector<uint8>& GetEncodedLayout()    const { return *GetDeviceObjectFactory().LookupResourceLayoutEncoding(m_hash); }



protected:
	std::vector<uint8>        EncodeDescriptorSet(const VectorMap<SResourceBindPoint, SResourceBinding>& resources);

	CDevice*                           m_pDevice;
	VkPipelineLayout                   m_pipelineLayout;

	uint64                             m_hash;
};

