// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include "xxhash.h"
#include "Vulkan/API/VKInstance.hpp"
#include "Vulkan/API/VKBufferResource.hpp"
#include "Vulkan/API/VKImageResource.hpp"
#include "Vulkan/API/VKSampler.hpp"
#include "Vulkan/API/VKExtensions.hpp"

#include <Common/Renderer.h>


#include "DeviceResourceSet_Vulkan.h"	


extern CD3D9Renderer gcpRendD3D;

using namespace NCryVulkan;

CDeviceResourceSet_Vulkan::~CDeviceResourceSet_Vulkan()
{
	ReleaseDescriptors();

	m_descriptorSetLayout.Destroy(vkDestroyDescriptorSetLayout, m_pDevice->GetVkDevice());
}

VkShaderStageFlags GetShaderStageFlags(EShaderStage shaderStages)
{
	VkShaderStageFlags shaderStageFlags = 0;

	if (shaderStages & EShaderStage_Vertex)
		shaderStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;

	if (shaderStages & EShaderStage_Pixel)
		shaderStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

	if (shaderStages & EShaderStage_Geometry)
		shaderStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;

	if (shaderStages & EShaderStage_Compute)
		shaderStageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;

	if (shaderStages & EShaderStage_Domain)
		shaderStageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

	if (shaderStages & EShaderStage_Hull)
		shaderStageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

	return shaderStageFlags;
}

bool CDeviceResourceSet_Vulkan::UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	if (uint8(dirtyFlags & CDeviceResourceSetDesc::EDirtyFlags::eDirtyBindPoint) || m_descriptorSetLayout == VK_NULL_HANDLE)
	{
		m_descriptorSetLayout.Destroy(vkDestroyDescriptorSetLayout, m_pDevice->GetVkDevice());
		m_descriptorSetLayout = CreateLayout(desc.GetResources());

		if (m_descriptorSetLayout == VK_NULL_HANDLE)
			return false;
	}

	if (m_pDescriptorSet != nullptr)
	{
		gcpRendD3D->m_DevBufMan.ReleaseDescriptorSet(m_pDescriptorSet);
	}

	m_pDescriptorSet = gcpRendD3D->m_DevBufMan.AllocateDescriptorSet(m_descriptorSetLayout);

	if (FillDescriptors(desc, m_pDescriptorSet))
	{
		m_descriptorSetHandle = m_pDescriptorSet->vkDescriptorSet;
		return true;
	}

	return true;
}

static const inline size_t NoAlign(size_t nSize) { return nSize; }

VkDescriptorSetLayout CDeviceResourceSet_Vulkan::CreateLayout(const VectorMap<SResourceBindPoint, SResourceBinding>& resources)
{
	CryStackAllocWithSizeVector(VkSampler, 16, immutableSamplers, NoAlign);
	CryStackAllocWithSizeVector(VkDescriptorSetLayoutBinding, resources.size(), layoutBindings, NoAlign);

	unsigned int samplerCount = 0;
	unsigned int descriptorCount = 0;

	for (auto it : resources)
	{
		const SResourceBindPoint& bindPoint = it.first;
		const SResourceBinding&   resource = it.second;

		layoutBindings[descriptorCount].binding = descriptorCount;
		layoutBindings[descriptorCount].descriptorCount = 1;
		layoutBindings[descriptorCount].stageFlags = GetShaderStageFlags(bindPoint.stages);
		layoutBindings[descriptorCount].descriptorType = GetDescriptorType(bindPoint);
		layoutBindings[descriptorCount].pImmutableSamplers = nullptr;

		if (resource.type == SResourceBinding::EResourceType::Sampler)
		{
			immutableSamplers[samplerCount] = CDeviceObjectFactory::LookupSamplerState(resource.samplerState).second->GetHandle();

			layoutBindings[descriptorCount].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			layoutBindings[descriptorCount].pImmutableSamplers = &immutableSamplers[samplerCount];

			++samplerCount;
		}

		++descriptorCount;
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo;
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.flags = 0;
	layoutInfo.bindingCount = descriptorCount;
	layoutInfo.pBindings = layoutBindings;

	VkDescriptorSetLayout layoutHandle;
	VkResult result = vkCreateDescriptorSetLayout(GetDevice()->GetVkDevice(), &layoutInfo, nullptr, &layoutHandle);

	CRY_ASSERT(result == VK_SUCCESS);
	return result == VK_SUCCESS ? layoutHandle : VK_NULL_HANDLE;
}

bool CDeviceResourceSet_Vulkan::FillDescriptors(const CDeviceResourceSetDesc& desc, const SDescriptorSet* pDescriptorSet)
{
	const unsigned int descriptorCount = desc.GetResources().size();
	m_inUseResources.clear();
	m_inUseResources.reserve(descriptorCount);

	CryStackAllocWithSizeVector(VkWriteDescriptorSet, descriptorCount, descriptorWrites, NoAlign);
	CryStackAllocWithSizeVector(VkDescriptorImageInfo, descriptorCount, descriptorImageInfos, NoAlign);
	CryStackAllocWithSizeVector(VkDescriptorBufferInfo, descriptorCount, descriptorBufferInfos, NoAlign);
	CryStackAllocWithSizeVector(VkBufferView, descriptorCount, descriptorBufferViews, NoAlign);

	unsigned int descriptorIndex = 0;
	unsigned int descriptorWriteIndex = 0;
	unsigned int imageInfoIndex = 0;
	unsigned int bufferInfoIndex = 0;
	unsigned int bufferViewIndex = 0;

	for (const auto& it : desc.GetResources())
	{
		const SResourceBindPoint& bindPoint = it.first;
		const SResourceBinding&   resource = it.second;

		if (!resource.IsValid())
		{
			CRY_ASSERT_MESSAGE(false, "Invalid resource in resource set desc. Update failed");
			return false;
		}

		if (resource.type != SResourceBinding::EResourceType::Sampler)
		{
			descriptorWrites[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[descriptorWriteIndex].pNext = nullptr;
			descriptorWrites[descriptorWriteIndex].dstSet = pDescriptorSet->vkDescriptorSet;
			descriptorWrites[descriptorWriteIndex].dstBinding = descriptorIndex;
			descriptorWrites[descriptorWriteIndex].dstArrayElement = 0;
			descriptorWrites[descriptorWriteIndex].descriptorCount = 1;
			descriptorWrites[descriptorWriteIndex].descriptorType = GetDescriptorType(bindPoint);
			descriptorWrites[descriptorWriteIndex].pImageInfo = nullptr;
			descriptorWrites[descriptorWriteIndex].pBufferInfo = nullptr;
			descriptorWrites[descriptorWriteIndex].pTexelBufferView = nullptr;

			switch (resource.type)
			{
			case SResourceBinding::EResourceType::ConstantBuffer:
			{
				buffer_size_t start, length;
				CBufferResource* pBuffer = resource.pConstantBuffer->GetD3D(&start, &length);
				m_inUseResources.emplace_back(pBuffer, false);

				VkDeviceSize offset = start;
				VkDeviceSize range = length > 0 ? length : (pBuffer->GetElementCount() * pBuffer->GetStride() - start);

				descriptorBufferInfos[bufferInfoIndex].buffer = pBuffer->GetHandle();
				descriptorBufferInfos[bufferInfoIndex].offset = offset;
				descriptorBufferInfos[bufferInfoIndex].range = range;

				descriptorWrites[descriptorWriteIndex].pBufferInfo = &descriptorBufferInfos[bufferInfoIndex];

				++bufferInfoIndex;
			}
			break;

			case SResourceBinding::EResourceType::Texture:
			{
				CImageView* const pView = static_cast<CImageView*>(resource.pTexture->GetDevTexture()->LookupResourceView(resource.view).second);

				const bool bBindAsSrv = it.first.slotType == SResourceBindPoint::ESlotType::TextureAndBuffer;
				m_inUseResources.emplace_back(pView->GetResource(), !bBindAsSrv);

				pView->FillInfo(descriptorImageInfos[imageInfoIndex], bBindAsSrv ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL);

				descriptorWrites[descriptorWriteIndex].pImageInfo = &descriptorImageInfos[imageInfoIndex];

				++imageInfoIndex;
			}
			break;

			case SResourceBinding::EResourceType::Buffer:
			{
				CBufferView* const pView = static_cast<CBufferView*>(resource.pBuffer->GetDevBuffer()->LookupResourceView(resource.view).second);

				const bool bBindAsSrv = it.first.slotType == SResourceBindPoint::ESlotType::TextureAndBuffer;
				m_inUseResources.emplace_back(pView->GetResource(), !bBindAsSrv);

				if (IsTexelBuffer(descriptorWrites[descriptorWriteIndex].descriptorType))
				{
					pView->FillInfo(descriptorBufferViews[bufferViewIndex]);

					descriptorWrites[descriptorWriteIndex].pBufferInfo = nullptr;
					descriptorWrites[descriptorWriteIndex].pTexelBufferView = &descriptorBufferViews[bufferViewIndex];

					++bufferViewIndex;
				}
				else
				{
					pView->FillInfo(descriptorBufferInfos[bufferInfoIndex]);

					descriptorWrites[descriptorWriteIndex].pBufferInfo = &descriptorBufferInfos[bufferInfoIndex];
					descriptorWrites[descriptorWriteIndex].pTexelBufferView = nullptr;

					++bufferInfoIndex;
				}
			}
			break;

			}

			++descriptorWriteIndex;
		}

		++descriptorIndex;
	}

	if (descriptorWriteIndex == 0)
	{
		return true;
	}

	vkUpdateDescriptorSets(m_pDevice->GetVkDevice(), descriptorWriteIndex, descriptorWrites, 0, nullptr);
	return true;
}

void CDeviceResourceSet_Vulkan::ReleaseDescriptors()
{
	if (m_pDescriptorSet != nullptr)
	{
		gcpRendD3D->m_DevBufMan.ReleaseDescriptorSet(m_pDescriptorSet);
		m_pDescriptorSet = nullptr;
	}

	m_inUseResources.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



CDeviceResourceLayout_Vulkan::~CDeviceResourceLayout_Vulkan()
{
	if(m_pDevice)
		vkDestroyPipelineLayout(m_pDevice->GetVkDevice(), m_pipelineLayout, nullptr);
	GetDeviceObjectFactory().UnRegisterEncodedResourceLayout(m_hash);
}

std::vector<uint8> CDeviceResourceLayout_Vulkan::EncodeDescriptorSet(const VectorMap<SResourceBindPoint, SResourceBinding>& resources)
{
	std::vector<uint8> result;
	result.reserve(1 + resources.size() * 2);

	result.push_back(resources.size());
	for (auto it : resources)
	{
		SResourceBindPoint& bindPoint = it.first;
		VkShaderStageFlags stages = GetShaderStageFlags(bindPoint.stages);
		uint8 descriptorCount = 1;

#ifndef _RELEASE
		if (
			(stages & 0x3F) != stages                                      ||
			((uint8)bindPoint.slotType & 0x3) != (uint8)bindPoint.slotType ||
			(descriptorCount & 0x3) != descriptorCount                     ||
			(bindPoint.slotNumber & 0x3F) != bindPoint.slotNumber
		)
		{
			CryFatalError("Encoding need to be changed. Stages flags, slot type, Descriptor count, and slot number are not fitting to two bytes anymore.");
		}
#endif
		uint8 slotTypeStageByte = ((uint8)bindPoint.slotType << 6) | (uint8)stages;
		result.push_back(slotTypeStageByte);              // 6-bits for stages + 2-bits for slotType
		uint8 slotNumberDescriptorCountByte = (descriptorCount << 6) | bindPoint.slotNumber;
		result.push_back(slotNumberDescriptorCountByte);  // 6-bits for slot number + 2-bits for descriptor count
	}

	return result;
}

bool CDeviceResourceLayout_Vulkan::Init(const SDeviceResourceLayoutDesc& desc)
{
	m_hash = 0;

	VkDescriptorSetLayout descriptorSets[EResourceLayoutSlot_Max] = {};
	std::vector<uint8>    encodedDescriptorSets[EResourceLayoutSlot_Max];

	uint8 descriptorSetCount = 0;

	for (auto& itLayoutBinding : desc.m_resourceBindings)
	{
		const auto& layoutBindPoint = itLayoutBinding.first;
		const auto& resources = itLayoutBinding.second;

		switch (layoutBindPoint.slotType)
		{
		case SDeviceResourceLayoutDesc::ELayoutSlotType::InlineConstantBuffer:
		{
			descriptorSets[layoutBindPoint.layoutSlot] = GetDeviceObjectFactory().GetInlineConstantBufferLayout();
		}
		break;

		case SDeviceResourceLayoutDesc::ELayoutSlotType::ResourceSet:
		{
			descriptorSets[layoutBindPoint.layoutSlot] = CDeviceResourceSet_Vulkan::CreateLayout(itLayoutBinding.second);
		}
		break;

		default:
			CRY_ASSERT(false);
		}

		encodedDescriptorSets[layoutBindPoint.layoutSlot] = EncodeDescriptorSet(resources);
		descriptorSetCount = max(descriptorSetCount, layoutBindPoint.layoutSlot);

	}

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = descriptorSetCount + 1;
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSets;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(m_pDevice->GetVkDevice(), &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);

	if (result == VK_SUCCESS)
	{
		m_hash = desc.GetHash();

		std::vector<uint8> encodedLayout;
		encodedLayout.reserve(MAX_TMU * 2);
		encodedLayout.push_back(static_cast<uint8>(desc.m_resourceBindings.size()));

		for (auto& encodedSet : encodedDescriptorSets)
			encodedLayout.insert(encodedLayout.end(), encodedSet.begin(), encodedSet.end());

		auto storedEncodedLayoutForHash = GetDeviceObjectFactory().LookupResourceLayoutEncoding(m_hash);
		if (storedEncodedLayoutForHash != nullptr)
		{
			CRY_ASSERT(std::mismatch(storedEncodedLayoutForHash->begin(), storedEncodedLayoutForHash->end(), encodedLayout.begin()).second == encodedLayout.end());
		}
		else
		{
			GetDeviceObjectFactory().RegisterEncodedResourceLayout(m_hash, std::move(encodedLayout));
		}
		
		return true;
	}

	return false;
}

