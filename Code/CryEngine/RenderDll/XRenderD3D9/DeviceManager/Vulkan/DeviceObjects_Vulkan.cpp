// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include "xxhash.h"
#include "Vulkan/API/VKInstance.hpp"
#include "Vulkan/API/VKBufferResource.hpp"
#include "Vulkan/API/VKImageResource.hpp"
#include "Vulkan/API/VKSampler.hpp"
#include "Vulkan/API/VKExtensions.hpp"

extern CD3D9Renderer gcpRendD3D;

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
		const SResourceBinding&   resource  = it.second;

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
		const SResourceBinding&   resource  = it.second;

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
	const std::vector<uint8>& GetEncodedLayout()    const { return m_encodedLayout; }

protected:
	std::vector<uint8>        EncodeDescriptorSet(const VectorMap<SResourceBindPoint, SResourceBinding>& resources);

	CDevice*                           m_pDevice;
	VkPipelineLayout                   m_pipelineLayout;

	uint64                             m_hash;
	std::vector<uint8>                 m_encodedLayout;
};

CDeviceResourceLayout_Vulkan::~CDeviceResourceLayout_Vulkan()
{
	vkDestroyPipelineLayout(m_pDevice->GetVkDevice(), m_pipelineLayout, nullptr);
}

std::vector<uint8> CDeviceResourceLayout_Vulkan::EncodeDescriptorSet(const VectorMap<SResourceBindPoint, SResourceBinding>& resources)
{
	std::vector<uint8> result;
	result.reserve(1 + resources.size() * 4);

	result.push_back(resources.size());
	for (auto it : resources)
	{
		SResourceBindPoint& bindPoint = it.first;
		VkShaderStageFlags stages = GetShaderStageFlags(bindPoint.stages);
		CRY_ASSERT((stages & 0xFF) == stages);

		result.push_back((uint8)bindPoint.slotType);
		result.push_back(bindPoint.slotNumber);
		result.push_back(1); // descriptor count
		result.push_back((uint8)stages);
	}

	return result;
}

bool CDeviceResourceLayout_Vulkan::Init(const SDeviceResourceLayoutDesc& desc)
{
	m_encodedLayout.clear();
	m_hash = 0;

	VkDescriptorSetLayout descriptorSets[EResourceLayoutSlot_Max] = {};
	std::vector<uint8>    encodedDescriptorSets[EResourceLayoutSlot_Max];

	uint8 descriptorSetCount = 0;

	for (auto& itLayoutBinding : desc.m_resourceBindings)
	{
		const auto& layoutBindPoint = itLayoutBinding.first;
		const auto& resources        = itLayoutBinding.second;

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
	pipelineLayoutCreateInfo.setLayoutCount = descriptorSetCount+1;
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSets;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(m_pDevice->GetVkDevice(), &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);

	if (result == VK_SUCCESS)
	{
		m_hash = desc.GetHash();

		m_encodedLayout.reserve(MAX_TMU * 4);
		m_encodedLayout.push_back(desc.m_resourceBindings.size());

		for (auto& encodedSet : encodedDescriptorSets)
			m_encodedLayout.insert(m_encodedLayout.end(), encodedSet.begin(), encodedSet.end());

		if (GetDeviceObjectFactory().LookupResourceLayoutEncoding(m_hash) != nullptr)
		{
			CryFatalError("Resource layout hash collision!");
		}

		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceGraphicsPSO_Vulkan : public CDeviceGraphicsPSO
{
public:
	CDeviceGraphicsPSO_Vulkan(CDevice* pDevice)
		: m_pDevice(pDevice)
		, m_pipeline(VK_NULL_HANDLE)
	{}

	~CDeviceGraphicsPSO_Vulkan();

	virtual EInitResult Init(const CDeviceGraphicsPSODesc& desc) final;

	const VkPipeline&   GetVkPipeline() const { return m_pipeline; }

protected:
	CDevice*   m_pDevice;
	VkPipeline m_pipeline;
};

CDeviceGraphicsPSO_Vulkan::~CDeviceGraphicsPSO_Vulkan()
{
	if (m_pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(m_pDevice->GetVkDevice(), m_pipeline, nullptr);
	}
}

CDeviceGraphicsPSO::EInitResult CDeviceGraphicsPSO_Vulkan::Init(const CDeviceGraphicsPSODesc& psoDesc)
{
	m_bValid = false;
	m_nUpdateCount++;

	if (psoDesc.m_pResourceLayout == nullptr || psoDesc.m_pShader == nullptr)
		return EInitResult::Failure;

	uint64 resourceLayoutHash = reinterpret_cast<CDeviceResourceLayout_Vulkan*>(psoDesc.m_pResourceLayout.get())->GetHash();
	UPipelineState customPipelineState[] = { resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash };
	auto hwShaders = SDeviceObjectHelpers::GetShaderInstanceInfo(psoDesc.m_pShader, psoDesc.m_technique, psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, customPipelineState, psoDesc.m_bAllowTesselation);

	// Vertex shader is required, both tessellation shaders should be used or omitted.
	if (hwShaders[eHWSC_Vertex].pHwShader == nullptr || (!(hwShaders[eHWSC_Domain].pHwShader == nullptr && hwShaders[eHWSC_Hull].pHwShader == nullptr) && !(hwShaders[eHWSC_Domain].pHwShader != nullptr && hwShaders[eHWSC_Hull].pHwShader != nullptr)))
		return EInitResult::Failure;

	const bool bDiscardRasterizer = (hwShaders[eHWSC_Pixel].pHwShader == nullptr) && (psoDesc.m_RenderState & GS_NODEPTHTEST) && !(psoDesc.m_RenderState & GS_DEPTHWRITE) && !(psoDesc.m_RenderState & GS_STENCIL);

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[eHWSC_Num];

	int validShaderCount = 0;
	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		m_pHwShaderInstances[shaderClass] = hwShaders[shaderClass].pHwShaderInstance;

		if (hwShaders[shaderClass].pHwShader == nullptr)
			continue;

		if (hwShaders[shaderClass].pHwShaderInstance == nullptr)
			return EInitResult::Failure;

		VkShaderStageFlagBits stage;
		switch (shaderClass)
		{
		case eHWSC_Vertex:
			stage = VK_SHADER_STAGE_VERTEX_BIT;
			break;
		case eHWSC_Pixel:
			stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			break;
		case eHWSC_Geometry:
			stage = VK_SHADER_STAGE_GEOMETRY_BIT;
			break;
		case eHWSC_Domain:
			stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			break;
		case eHWSC_Hull:
			stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			break;
		default:
			CRY_ASSERT_MESSAGE(0, "Shader class not supported");
			return EInitResult::Failure;
		}

		shaderStageCreateInfos[validShaderCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfos[validShaderCount].pNext = nullptr;
		shaderStageCreateInfos[validShaderCount].flags = 0;
		shaderStageCreateInfos[validShaderCount].stage = stage;
		shaderStageCreateInfos[validShaderCount].module = reinterpret_cast<NCryVulkan::CShader*>(hwShaders[shaderClass].pDeviceShader)->GetVulkanShader();
		shaderStageCreateInfos[validShaderCount].pName = "main"; // SPIR-V compiler (glslangValidator) currently supports this only
		shaderStageCreateInfos[validShaderCount].pSpecializationInfo = nullptr;

		validShaderCount++;
	}

	TArray<VkVertexInputBindingDescription> vertexInputBindingDescriptions;
	TArray<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;

	// input layout
	if (psoDesc.m_VertexFormat != EDefaultInputLayouts::Empty)
	{
		if (psoDesc.m_VertexFormat == EDefaultInputLayouts::Unspecified)
			return EInitResult::Failure;

		if (CHWShader_D3D::SHWSInstance* pVsInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(hwShaders[eHWSC_Vertex].pHwShaderInstance))
		{
			const VkPhysicalDeviceLimits& limits = GetDevice()->GetPhysicalDeviceInfo()->deviceProperties.limits;
			vertexInputBindingDescriptions.reserve(min(limits.maxVertexInputBindings, 16u));
			vertexInputAttributeDescriptions.reserve(min(limits.maxVertexInputAttributes, 32u));
			std::vector<uint32> strides(limits.maxVertexInputBindings, 0);

			uint32 streamMask = psoDesc.CombineVertexStreamMasks(uint8(pVsInstance->m_VStreamMask_Decl), psoDesc.m_ObjectStreamMask);

			auto& inputLayout = CDeviceObjectFactory::LookupInputLayout(CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&pVsInstance->m_Shader, streamMask, 0, 0, nullptr, psoDesc.m_VertexFormat)).first;
			const unsigned int declarationCount = inputLayout.m_Declaration.size();

			// match shader inputs to input layout by semantic
			for (const auto& declInputElement : inputLayout.m_Declaration)
			{
				// update stride for this slot
				CRY_ASSERT(declInputElement.InputSlot < strides.size());
				strides[declInputElement.InputSlot] = max(strides[declInputElement.InputSlot], declInputElement.AlignedByteOffset + DeviceFormats::GetStride(declInputElement.Format));

				for (const auto& vsInputElement : pVsInstance->m_VSInputStreams)
				{
					if (strcmp(vsInputElement.semanticName, declInputElement.SemanticName) == 0 &&
						vsInputElement.semanticIndex == declInputElement.SemanticIndex)
					{
						// check if we already have a binding for this slot
						auto itBinding = vertexInputBindingDescriptions.begin();
						for (; itBinding != vertexInputBindingDescriptions.end(); ++itBinding)
						{
							if (itBinding->binding == declInputElement.InputSlot)
							{
								CRY_ASSERT_MESSAGE(itBinding->inputRate == (declInputElement.InputSlotClass == D3D11_INPUT_PER_VERTEX_DATA ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE), "Mismatching input rate not supported");
								break;
							}
						}

						// need to add a new binding
						if (itBinding == vertexInputBindingDescriptions.end())
						{
							CRY_ASSERT_MESSAGE(declInputElement.InputSlotClass == D3D11_INPUT_PER_VERTEX_DATA || declInputElement.InstanceDataStepRate == 1, "Data step rate not supported");

							VkVertexInputBindingDescription bindingDesc;
							bindingDesc.binding = declInputElement.InputSlot;
							bindingDesc.stride = 0;
							bindingDesc.inputRate = declInputElement.InputSlotClass == D3D11_INPUT_PER_VERTEX_DATA ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
							
							vertexInputBindingDescriptions.push_back(bindingDesc);
							itBinding = vertexInputBindingDescriptions.end() - 1;
						}

						// add input attribute
						VkVertexInputAttributeDescription attributeDesc;
						attributeDesc.location = vsInputElement.attributeLocation;
						attributeDesc.binding = itBinding->binding;
						attributeDesc.format = s_FormatWithSize[declInputElement.Format].Format;
						attributeDesc.offset = declInputElement.AlignedByteOffset;

						vertexInputAttributeDescriptions.push_back(attributeDesc);
						break;
					}
				}
			}

			// update binding strides
			for (auto& binding : vertexInputBindingDescriptions)
				binding.stride = strides[binding.binding];
		}
	}

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = vertexInputBindingDescriptions.Num();
	vertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindingDescriptions.Data();
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = vertexInputAttributeDescriptions.Num();
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions.Data();

	if (!ValidateShadersAndTopologyCombination(psoDesc, m_pHwShaderInstances))
		return EInitResult::ErrorShadersAndTopologyCombination;

	VkPrimitiveTopology topology;
	unsigned int controlPointCount = 0;
	switch (psoDesc.m_PrimitiveType)
	{
	case eptTriangleList:
		topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		break;
	case eptTriangleStrip:
		topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		break;
	case eptLineList:
		topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		break;
	case eptLineStrip:
		topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		break;
	case eptPointList:
		topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		break;
	case ept1ControlPointPatchList:
		topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		controlPointCount = 1;
		break;
	case ept2ControlPointPatchList:
		topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		controlPointCount = 2;
		break;
	case ept3ControlPointPatchList:
		topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		controlPointCount = 3;
		break;
	case ept4ControlPointPatchList:
		topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		controlPointCount = 4;
		break;
	default:
		CRY_ASSERT_MESSAGE(0, "Primitive type not supported");
		return EInitResult::Failure;
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = topology;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo = {};
	tessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tessellationStateCreateInfo.pNext = nullptr;
	tessellationStateCreateInfo.flags = 0;
	tessellationStateCreateInfo.patchControlPoints = controlPointCount;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = nullptr;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = nullptr;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = psoDesc.m_bDepthClip ? VK_FALSE : VK_TRUE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = bDiscardRasterizer ? VK_TRUE : VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = (psoDesc.m_RenderState & GS_WIREFRAME) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = (psoDesc.m_CullMode == eCULL_Back) ? VK_CULL_MODE_BACK_BIT : (psoDesc.m_CullMode == eCULL_None) ? VK_CULL_MODE_NONE : VK_CULL_MODE_FRONT_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0;
	rasterizationStateCreateInfo.depthBiasClamp = 0;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	static VkCompareOp DepthCompareOp[GS_DEPTHFUNC_MASK >> GS_DEPTHFUNC_SHIFT] =
	{
		VK_COMPARE_OP_LESS_OR_EQUAL,     // GS_DEPTHFUNC_LEQUAL
		VK_COMPARE_OP_EQUAL,             // GS_DEPTHFUNC_EQUAL
		VK_COMPARE_OP_GREATER,           // GS_DEPTHFUNC_GREAT
		VK_COMPARE_OP_LESS,              // GS_DEPTHFUNC_LESS
		VK_COMPARE_OP_GREATER_OR_EQUAL,  // GS_DEPTHFUNC_GEQUAL
		VK_COMPARE_OP_NOT_EQUAL,         // GS_DEPTHFUNC_NOTEQUAL
	};

	static VkCompareOp StencilCompareOp[8] =
	{
		VK_COMPARE_OP_ALWAYS,           // FSS_STENCFUNC_ALWAYS   0x0
		VK_COMPARE_OP_NEVER,            // FSS_STENCFUNC_NEVER    0x1
		VK_COMPARE_OP_LESS,             // FSS_STENCFUNC_LESS     0x2
		VK_COMPARE_OP_LESS_OR_EQUAL,    // FSS_STENCFUNC_LEQUAL   0x3
		VK_COMPARE_OP_GREATER,          // FSS_STENCFUNC_GREATER  0x4
		VK_COMPARE_OP_GREATER_OR_EQUAL, // FSS_STENCFUNC_GEQUAL   0x5
		VK_COMPARE_OP_EQUAL,            // FSS_STENCFUNC_EQUAL    0x6
		VK_COMPARE_OP_NOT_EQUAL         // FSS_STENCFUNC_NOTEQUAL 0x7
	};

	static VkStencilOp StencilOp[8] =
	{
		VK_STENCIL_OP_KEEP,                // FSS_STENCOP_KEEP    0x0
		VK_STENCIL_OP_REPLACE,             // FSS_STENCOP_REPLACE 0x1
		VK_STENCIL_OP_INCREMENT_AND_CLAMP, // FSS_STENCOP_INCR    0x2
		VK_STENCIL_OP_DECREMENT_AND_CLAMP, // FSS_STENCOP_DECR    0x3
		VK_STENCIL_OP_ZERO,                // FSS_STENCOP_ZERO    0x4
		VK_STENCIL_OP_INCREMENT_AND_WRAP,  // FSS_STENCOP_INCR_WRAP 0x5
		VK_STENCIL_OP_DECREMENT_AND_WRAP,  // FSS_STENCOP_DECR_WRAP 0x6
		VK_STENCIL_OP_INVERT               // FSS_STENCOP_INVERT 0x7
	};

	VkCompareOp depthCompareOp = (psoDesc.m_RenderState & (GS_NODEPTHTEST | GS_DEPTHWRITE)) == (GS_NODEPTHTEST | GS_DEPTHWRITE)
		? VK_COMPARE_OP_ALWAYS
		: DepthCompareOp[(psoDesc.m_RenderState & GS_DEPTHFUNC_MASK) >> GS_DEPTHFUNC_SHIFT];

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable  = ((psoDesc.m_RenderState & GS_NODEPTHTEST) && !(psoDesc.m_RenderState & GS_DEPTHWRITE)) ? VK_FALSE : VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = (psoDesc.m_RenderState & GS_DEPTHWRITE) ? VK_TRUE : VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = (psoDesc.m_RenderState & GS_STENCIL) ? VK_TRUE : VK_FALSE;

	depthStencilStateCreateInfo.front.failOp = StencilOp[(psoDesc.m_StencilState & FSS_STENCFAIL_MASK) >> FSS_STENCFAIL_SHIFT];
	depthStencilStateCreateInfo.front.passOp = StencilOp[(psoDesc.m_StencilState & FSS_STENCPASS_MASK) >> FSS_STENCPASS_SHIFT];
	depthStencilStateCreateInfo.front.depthFailOp = StencilOp[(psoDesc.m_StencilState & FSS_STENCZFAIL_MASK) >> FSS_STENCZFAIL_SHIFT];
	depthStencilStateCreateInfo.front.compareOp = StencilCompareOp[psoDesc.m_StencilState & FSS_STENCFUNC_MASK];
	depthStencilStateCreateInfo.front.compareMask = psoDesc.m_StencilReadMask;
	depthStencilStateCreateInfo.front.writeMask = psoDesc.m_StencilWriteMask;
	depthStencilStateCreateInfo.front.reference = 0xFFFFFFFF;

	depthStencilStateCreateInfo.back = depthStencilStateCreateInfo.front;
	if (psoDesc.m_StencilState & FSS_STENCIL_TWOSIDED)
	{
		depthStencilStateCreateInfo.back.failOp = StencilOp[(psoDesc.m_StencilState & (FSS_STENCFAIL_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCFAIL_SHIFT + FSS_CCW_SHIFT)];
		depthStencilStateCreateInfo.back.passOp = StencilOp[(psoDesc.m_StencilState & (FSS_STENCPASS_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCPASS_SHIFT + FSS_CCW_SHIFT)];
		depthStencilStateCreateInfo.back.depthFailOp = StencilOp[(psoDesc.m_StencilState & (FSS_STENCZFAIL_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCZFAIL_SHIFT + FSS_CCW_SHIFT)];
		depthStencilStateCreateInfo.back.compareOp = StencilCompareOp[(psoDesc.m_StencilState & (FSS_STENCFUNC_MASK << FSS_CCW_SHIFT)) >> FSS_CCW_SHIFT];
	}

	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	struct SBlendFactors
	{
		VkBlendFactor BlendColor;
		VkBlendFactor BlendAlpha;
	};

	static SBlendFactors SrcBlendFactors[GS_BLSRC_MASK >> GS_BLSRC_SHIFT] =
	{
		{ (VkBlendFactor)0,                    (VkBlendFactor)0                    }, // UNINITIALIZED BLEND FACTOR
		{ VK_BLEND_FACTOR_ZERO,                VK_BLEND_FACTOR_ZERO                }, // GS_BLSRC_ZERO
		{ VK_BLEND_FACTOR_ONE,                 VK_BLEND_FACTOR_ONE                 }, // GS_BLSRC_ONE
		{ VK_BLEND_FACTOR_DST_COLOR,           VK_BLEND_FACTOR_DST_ALPHA           }, // GS_BLSRC_DSTCOL
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA }, // GS_BLSRC_ONEMINUSDSTCOL
		{ VK_BLEND_FACTOR_SRC_ALPHA,           VK_BLEND_FACTOR_SRC_ALPHA           }, // GS_BLSRC_SRCALPHA
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA }, // GS_BLSRC_ONEMINUSSRCALPHA
		{ VK_BLEND_FACTOR_DST_ALPHA,           VK_BLEND_FACTOR_DST_ALPHA           }, // GS_BLSRC_DSTALPHA
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA }, // GS_BLSRC_ONEMINUSDSTALPHA
		{ VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,  VK_BLEND_FACTOR_SRC_ALPHA_SATURATE  }, // GS_BLSRC_ALPHASATURATE
		{ VK_BLEND_FACTOR_SRC_ALPHA,           VK_BLEND_FACTOR_ZERO                }, // GS_BLSRC_SRCALPHA_A_ZERO
		{ VK_BLEND_FACTOR_SRC1_ALPHA,          VK_BLEND_FACTOR_SRC1_ALPHA          }, // GS_BLSRC_SRC1ALPHA
	};

	static SBlendFactors DstBlendFactors[GS_BLDST_MASK >> GS_BLDST_SHIFT] =
	{
		{ (VkBlendFactor)0,                     (VkBlendFactor)0                     }, // UNINITIALIZED BLEND FACTOR
		{ VK_BLEND_FACTOR_ZERO,                 VK_BLEND_FACTOR_ZERO                 }, // GS_BLDST_ZERO
		{ VK_BLEND_FACTOR_ONE,                  VK_BLEND_FACTOR_ONE                  }, // GS_BLDST_ONE
		{ VK_BLEND_FACTOR_SRC_COLOR,            VK_BLEND_FACTOR_SRC_ALPHA            }, // GS_BLDST_SRCCOL
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA  }, // GS_BLDST_ONEMINUSSRCCOL
		{ VK_BLEND_FACTOR_SRC_ALPHA,            VK_BLEND_FACTOR_SRC_ALPHA            }, // GS_BLDST_SRCALPHA
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA  }, // GS_BLDST_ONEMINUSSRCALPHA
		{ VK_BLEND_FACTOR_DST_ALPHA,            VK_BLEND_FACTOR_DST_ALPHA            }, // GS_BLDST_DSTALPHA
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA  }, // GS_BLDST_ONEMINUSDSTALPHA
		{ VK_BLEND_FACTOR_ONE,                  VK_BLEND_FACTOR_ZERO                 }, // GS_BLDST_ONE_A_ZERO
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA }, // GS_BLDST_ONEMINUSSRC1ALPHA
	};

	static VkBlendOp BlendOp[GS_BLEND_OP_MASK >> GS_BLEND_OP_SHIFT] =
	{
		VK_BLEND_OP_ADD,              // 0 (unspecified): Default
		VK_BLEND_OP_MAX,              // GS_BLOP_MAX / GS_BLALPHA_MAX
		VK_BLEND_OP_MIN,              // GS_BLOP_MIN / GS_BLALPHA_MIN
		VK_BLEND_OP_SUBTRACT,         // GS_BLOP_SUB / GS_BLALPHA_SUB
		VK_BLEND_OP_REVERSE_SUBTRACT, // GS_BLOP_SUBREV / GS_BLALPHA_SUBREV
	};

	uint32 colorWriteMask = 0xfffffff0 | ((psoDesc.m_RenderState & GS_COLMASK_MASK) >> GS_COLMASK_SHIFT);
	colorWriteMask = (~colorWriteMask) & 0xf;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[CD3D9Renderer::RT_STACK_WIDTH];

	int validBlendAttachmentStateCount = 0;

	const CDeviceRenderPassDesc* pRenderPassDesc = GetDeviceObjectFactory().GetRenderPassDesc(psoDesc.m_pRenderPass.get());
	if (!pRenderPassDesc)
		return EInitResult::Failure;

	const auto& renderTargets = pRenderPassDesc->GetRenderTargets();

	for (int i = 0; i < renderTargets.size(); i++)
	{
		if (!renderTargets[i].pTexture)
			break;

		colorBlendAttachmentStates[validBlendAttachmentStateCount].blendEnable = (psoDesc.m_RenderState & GS_BLEND_MASK) ? VK_TRUE : VK_FALSE;
		colorBlendAttachmentStates[validBlendAttachmentStateCount].srcColorBlendFactor = SrcBlendFactors[(psoDesc.m_RenderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].BlendColor;
		colorBlendAttachmentStates[validBlendAttachmentStateCount].dstColorBlendFactor = DstBlendFactors[(psoDesc.m_RenderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].BlendColor;
		colorBlendAttachmentStates[validBlendAttachmentStateCount].colorBlendOp = BlendOp[(psoDesc.m_RenderState & GS_BLEND_OP_MASK) >> GS_BLEND_OP_SHIFT];
		colorBlendAttachmentStates[validBlendAttachmentStateCount].srcAlphaBlendFactor = SrcBlendFactors[(psoDesc.m_RenderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].BlendAlpha;
		colorBlendAttachmentStates[validBlendAttachmentStateCount].dstAlphaBlendFactor = DstBlendFactors[(psoDesc.m_RenderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].BlendAlpha;
		colorBlendAttachmentStates[validBlendAttachmentStateCount].alphaBlendOp = BlendOp[(psoDesc.m_RenderState & GS_BLALPHA_MASK) >> GS_BLALPHA_SHIFT];
		colorBlendAttachmentStates[validBlendAttachmentStateCount].colorWriteMask = DeviceFormats::GetWriteMask(renderTargets[i].GetResourceFormat()) & colorWriteMask;

		if ((psoDesc.m_RenderState & GS_BLALPHA_MASK) == GS_BLALPHA_MIN)
		{
			colorBlendAttachmentStates[validBlendAttachmentStateCount].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentStates[validBlendAttachmentStateCount].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		}

		validBlendAttachmentStateCount++;
	}

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_CLEAR;
	colorBlendStateCreateInfo.attachmentCount = validBlendAttachmentStateCount;
	colorBlendStateCreateInfo.pAttachments = colorBlendAttachmentStates;
	colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	static const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE, VK_DYNAMIC_STATE_DEPTH_BIAS };
	static const unsigned int dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	
	const bool bDisableDynamicDepthBias = !psoDesc.m_bDynamicDepthBias;

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = dynamicStateCount - bDisableDynamicDepthBias;
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = nullptr;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = validShaderCount;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos;
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = hwShaders[eHWSC_Hull].pHwShader != nullptr ? &tessellationStateCreateInfo : nullptr;
	graphicsPipelineCreateInfo.pViewportState = bDiscardRasterizer == false ? &viewportStateCreateInfo : nullptr;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = bDiscardRasterizer == false ? &multisampleStateCreateInfo : nullptr;
	graphicsPipelineCreateInfo.pDepthStencilState = bDiscardRasterizer == false ? &depthStencilStateCreateInfo : nullptr;
	graphicsPipelineCreateInfo.pColorBlendState = (bDiscardRasterizer == false && validBlendAttachmentStateCount != 0) ? &colorBlendStateCreateInfo : nullptr;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = static_cast<CDeviceResourceLayout_Vulkan*>(psoDesc.m_pResourceLayout.get())->GetVkPipelineLayout();
	graphicsPipelineCreateInfo.renderPass = psoDesc.m_pRenderPass->m_renderPassHandle;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;

	VkResult result = vkCreateGraphicsPipelines(m_pDevice->GetVkDevice(), m_pDevice->GetVkPipelineCache(), 1, &graphicsPipelineCreateInfo, nullptr, &m_pipeline);

	if (result != VK_SUCCESS)
		return EInitResult::Failure;

#if defined(ENABLE_PROFILING_CODE)
	m_PrimitiveTypeForProfiling = psoDesc.m_PrimitiveType;
#endif

	m_bValid = true;

	return EInitResult::Success;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceComputePSO_Vulkan : public CDeviceComputePSO
{
public:
	CDeviceComputePSO_Vulkan(CDevice* pDevice)
		: m_pDevice(pDevice)
		, m_pipeline(VK_NULL_HANDLE)
	{}

	~CDeviceComputePSO_Vulkan();

	virtual bool      Init(const CDeviceComputePSODesc& desc) final;

	const VkPipeline& GetVkPipeline() const { return m_pipeline; }

protected:
	CDevice*   m_pDevice;
	VkPipeline m_pipeline;
};

CDeviceComputePSO_Vulkan::~CDeviceComputePSO_Vulkan()
{
	if (m_pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(m_pDevice->GetVkDevice(), m_pipeline, nullptr);
	}
}

bool CDeviceComputePSO_Vulkan::Init(const CDeviceComputePSODesc& psoDesc)
{
	m_bValid = false;
	m_nUpdateCount++;

	if (psoDesc.m_pResourceLayout == nullptr || psoDesc.m_pShader == nullptr)
		return false;

	uint64 resourceLayoutHash = reinterpret_cast<CDeviceResourceLayout_Vulkan*>(psoDesc.m_pResourceLayout.get())->GetHash();
	UPipelineState customPipelineState[] = { resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash };
	auto hwShaders = SDeviceObjectHelpers::GetShaderInstanceInfo(psoDesc.m_pShader, psoDesc.m_technique, psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, customPipelineState, false);

	if (hwShaders[eHWSC_Compute].pHwShader == nullptr || hwShaders[eHWSC_Compute].pHwShaderInstance == nullptr)
		return false;

	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.flags = 0;
	computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computePipelineCreateInfo.stage.pNext = nullptr;
	computePipelineCreateInfo.stage.flags = 0;
	computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computePipelineCreateInfo.stage.module = reinterpret_cast<NCryVulkan::CShader*>(hwShaders[eHWSC_Compute].pDeviceShader)->GetVulkanShader();
	computePipelineCreateInfo.stage.pName = "main"; // SPIR-V compiler (glslangValidator) currently supports this only
	computePipelineCreateInfo.stage.pSpecializationInfo = nullptr;
	computePipelineCreateInfo.layout = static_cast<CDeviceResourceLayout_Vulkan*>(psoDesc.m_pResourceLayout.get())->GetVkPipelineLayout();
	computePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	computePipelineCreateInfo.basePipelineIndex = 0;

	VkResult result = vkCreateComputePipelines(m_pDevice->GetVkDevice(), m_pDevice->GetVkPipelineCache(), 1, &computePipelineCreateInfo, nullptr, &m_pipeline);

	if (result != VK_SUCCESS)
		return false;

	m_pHwShaderInstance = hwShaders[eHWSC_Compute].pHwShaderInstance;
	m_bValid = true;

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceTimestampGroup::CDeviceTimestampGroup()
	: m_numTimestamps(0)
	, m_queryPool(VK_NULL_HANDLE)
	, m_fence(0)
{}

CDeviceTimestampGroup::~CDeviceTimestampGroup()
{
	vkDestroyQueryPool(GetDevice()->GetVkDevice(), m_queryPool, nullptr);
	GetDeviceObjectFactory().ReleaseFence(m_fence);
}

void CDeviceTimestampGroup::Init()
{
	GetDeviceObjectFactory().CreateFence(m_fence);

	VkQueryPoolCreateInfo poolInfo;
	poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = 0;
	poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	poolInfo.queryCount = kMaxTimestamps;
	poolInfo.pipelineStatistics = 0;

	VkResult result = vkCreateQueryPool(GetDevice()->GetVkDevice(), &poolInfo, nullptr, &m_queryPool);
	CRY_ASSERT(result == VK_SUCCESS);
}

void CDeviceTimestampGroup::BeginMeasurement()
{
	CDeviceCommandListRef coreCommandList = GetDeviceObjectFactory().GetCoreCommandList();
	vkCmdResetQueryPool(coreCommandList.GetVKCommandList()->GetVkCommandList(), m_queryPool, 0, kMaxTimestamps);

	m_numTimestamps = 0;
}

void CDeviceTimestampGroup::EndMeasurement()
{
	GetDeviceObjectFactory().IssueFence(m_fence);
}

uint32 CDeviceTimestampGroup::IssueTimestamp(void* pCommandList)
{
	CRY_ASSERT(m_numTimestamps < kMaxTimestamps);

	CDeviceCommandListRef deviceCommandList = pCommandList ? *reinterpret_cast<CDeviceCommandList*>(pCommandList) : GetDeviceObjectFactory().GetCoreCommandList();
	vkCmdWriteTimestamp(deviceCommandList.GetVKCommandList()->GetVkCommandList(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPool, m_numTimestamps);

	return m_numTimestamps++;
}

bool CDeviceTimestampGroup::ResolveTimestamps()
{
	if (m_numTimestamps == 0)
		return true;

	if (GetDeviceObjectFactory().SyncFence(m_fence, false, true) == S_OK)
	{
		if (vkGetQueryPoolResults(GetDevice()->GetVkDevice(), m_queryPool, 0, m_numTimestamps, sizeof(m_timestampData), m_timestampData.data(), sizeof(uint64), VK_QUERY_RESULT_64_BIT)== VK_SUCCESS)
			return true;
	}

	return false;
}

float CDeviceTimestampGroup::GetTimeMS(uint32 timestamp0, uint32 timestamp1)
{
	const float ticksToNanoseconds = GetDevice()->GetPhysicalDeviceInfo()->deviceProperties.limits.timestampPeriod;

	uint64 ticks = std::max(m_timestampData[timestamp0], m_timestampData[timestamp1]) - std::min(m_timestampData[timestamp0], m_timestampData[timestamp1]);
	return ticks * ticksToNanoseconds / 1000000.0f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeviceCommandListImpl::SetProfilerMarker(const char* label)
{
	if (Extensions::EXT_debug_marker::IsSupported)
	{
		VkDebugMarkerMarkerInfoEXT markerInfo;
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.pNext = nullptr;
		markerInfo.pMarkerName = label;
		ZeroArray(markerInfo.color);

		Extensions::EXT_debug_marker::CmdDebugMarkerInsert(GetVKCommandList()->GetVkCommandList(), &markerInfo);
	}
}

void CDeviceCommandListImpl::BeginProfilerEvent(const char* label)
{
	if (Extensions::EXT_debug_marker::IsSupported)
	{
		VkDebugMarkerMarkerInfoEXT markerInfo;
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.pNext = nullptr;
		markerInfo.pMarkerName = label;
		ZeroArray(markerInfo.color);

		Extensions::EXT_debug_marker::CmdDebugMarkerBegin(GetVKCommandList()->GetVkCommandList(), &markerInfo);
	}
}

void CDeviceCommandListImpl::EndProfilerEvent(const char* label)
{
	if (Extensions::EXT_debug_marker::IsSupported)
	{
		Extensions::EXT_debug_marker::CmdDebugMarkerEnd(GetVKCommandList()->GetVkCommandList());
	}
}

void CDeviceCommandListImpl::RequestTransition(CImageResource* pResource, VkImageLayout desiredLayout, VkAccessFlags desiredAccess) const
{
	pResource->TrackAnnouncedAccess(desiredAccess);
	pResource->TransitionBarrier(GetVKCommandList(), desiredLayout);
}

void CDeviceCommandListImpl::RequestTransition(NCryVulkan::CBufferResource* pResource, VkAccessFlags desiredAccess) const
{
	pResource->TrackAnnouncedAccess(desiredAccess);
	pResource->RangeBarrier(GetVKCommandList());
}

CDeviceCommandListImpl::~CDeviceCommandListImpl()
{
}

static auto lambdaCeaseCallback = [](void* cmd, uint nPoolId)
{
	reinterpret_cast<CDeviceCommandListImpl*>(cmd)->CeaseCommandListEvent(nPoolId);
};

static auto lambdaResumeCallback = [](void* cmd, uint nPoolId)
{
	reinterpret_cast<CDeviceCommandListImpl*>(cmd)->ResumeCommandListEvent(nPoolId);
};

void CDeviceCommandListImpl::CeaseCommandListEvent(int nPoolId)
{
	if (nPoolId != CMDQUEUE_GRAPHICS)
		return;

	auto* pCommandList = GetScheduler()->GetCommandList(nPoolId);
	CRY_ASSERT(m_sharedState.pCommandList == pCommandList || m_sharedState.pCommandList == nullptr);
	m_sharedState.pCommandList = nullptr;

#if defined(ENABLE_FRAME_PROFILER_LABELS)
	for (auto pEventLabel : m_profilerEventStack)
	{
		VK_NOT_IMPLEMENTED;
	}
#endif

	reinterpret_cast<CDeviceCommandList*>(this)->Reset();
}

void CDeviceCommandListImpl::ResumeCommandListEvent(int nPoolId)
{
	if (nPoolId != CMDQUEUE_GRAPHICS)
		return;

	auto* pCommandList = GetScheduler()->GetCommandList(nPoolId);
	CRY_ASSERT(m_sharedState.pCommandList == nullptr);
	m_sharedState.pCommandList = pCommandList;

#if defined(ENABLE_FRAME_PROFILER_LABELS)
	for (auto pEventLabel : m_profilerEventStack)
	{
		VK_NOT_IMPLEMENTED;
	}
#endif

	if (CDeviceGraphicsCommandInterfaceImpl* pGraphicsInterface = GetGraphicsInterfaceImpl())
	{
		pGraphicsInterface->BindNullVertexBuffers();
	}
}

void CDeviceCommandListImpl::ResetImpl()
{
	m_graphicsState.custom.pendingBindings.Reset();
	m_computeState.custom.pendingBindings.Reset();
}

void CDeviceCommandListImpl::LockToThreadImpl()
{
	NCryVulkan::CCommandList* pCommandListVK = GetVKCommandList();

	pCommandListVK->Begin();

	if (CDeviceGraphicsCommandInterfaceImpl* pGraphicsInterface = GetGraphicsInterfaceImpl())
	{
		pGraphicsInterface->BindNullVertexBuffers();
	}
}

void CDeviceCommandListImpl::CloseImpl()
{
	FUNCTION_PROFILER_RENDERER
	GetVKCommandList()->End();
}

void CDeviceCommandListImpl::ApplyPendingBindings(VkCommandBuffer vkCommandList, VkPipelineLayout vkPipelineLayout, VkPipelineBindPoint vkPipelineBindPoint, const SPendingBindings& RESTRICT_REFERENCE bindings)
{
	uint32 toBind = bindings.validMask;

	while (toBind != 0)
	{
		uint32 start  = countTrailingZeros32(toBind);
		uint32 curBit = (1 << start);

		const bool bStaticOffset = !(bindings.dynamicOffsetMask & curBit);

		while (true)
		{
			curBit <<= 1;

			if ((toBind & curBit) == 0)
				break;

			const bool curStaticOffset = !(bindings.dynamicOffsetMask & curBit);
			if (curStaticOffset != bStaticOffset)
				break;
		}
		
		uint32 end = countTrailingZeros32(curBit);
		toBind &= ~uint32(curBit - 1);

		vkCmdBindDescriptorSets(vkCommandList, vkPipelineBindPoint, vkPipelineLayout,
			start, end - start, &bindings.descriptorSets[start],
			bStaticOffset ? 0 : end - start, bStaticOffset ? nullptr : &bindings.dynamicOffsets[start]);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandInterfaceImpl::PrepareUAVsForUseImpl(uint32 viewCount, CGpuBuffer** pViews, bool bCompute) const
{
	for (uint32 i = 0; i < viewCount; ++i)
	{
		RequestTransition(pViews[i]->GetDevBuffer()->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareRenderPassForUseImpl(CDeviceRenderPass& renderPass) const
{
	if (renderPass.m_pDepthStencilTarget)
	{
		// TODO: Read-only DSV?
		RequestTransition(renderPass.m_pDepthStencilTarget, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
	}

	for (uint32 i = 0; i < renderPass.m_renderTargetCount; ++i)
	{
		RequestTransition(renderPass.m_renderTargets[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	CDeviceResourceSet_Vulkan* const pResourceSet = static_cast<CDeviceResourceSet_Vulkan*>(pResources);
	auto lambda = [this](CMemoryResource* pResource, bool bWritable)
	{
		if (CDynamicOffsetBufferResource* pBuffer = pResource->AsDynamicOffsetBuffer())
		{
			RequestTransition(pBuffer, VK_ACCESS_UNIFORM_READ_BIT);
		}
		else
		{
			const VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | (bWritable ? VK_ACCESS_SHADER_WRITE_BIT : 0);
			const VkImageLayout layout = bWritable ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			if (CBufferResource* pBuffer = pResource->AsBuffer())
			{
				RequestTransition(pBuffer, access);
			}
			else if (CImageResource* pImage = pResource->AsImage())
			{
				RequestTransition(pImage, layout, access);
			}
		}
	};
	pResourceSet->EnumerateInUseResources(lambda);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareResourceForUseImpl(uint32 bindSlot, CTexture* pTexture, const ResourceViewHandle TextureView, ::EShaderStage srvUsage) const
{
	const uint64_t viewKey = pTexture->GetDevTexture()->LookupResourceView(TextureView).first.m_Desc.Key;
	const bool bWritable = SResourceView::IsUnorderedAccessView(viewKey);
	const VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | (bWritable ? VK_ACCESS_SHADER_WRITE_BIT : 0);
	const VkImageLayout layout = bWritable ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	RequestTransition(pTexture->GetDevTexture()->Get2DTexture(), layout, access);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
	PrepareInlineConstantBufferForUseImpl(bindSlot, pBuffer, shaderSlot, SHADERSTAGE_FROM_SHADERCLASS(shaderClass));
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages) const
{
	buffer_size_t offset, bytes;
	CBufferResource* const pActualBuffer = pBuffer->GetD3D(&offset, &bytes);

	CRY_ASSERT(pActualBuffer->AsDynamicOffsetBuffer() != nullptr); // Needs to be a buffer with dynamic offsets
	RequestTransition(pActualBuffer, VK_ACCESS_UNIFORM_READ_BIT);

	if (pActualBuffer->AsDynamicOffsetBuffer()->GetDynamicDescriptorSet(bytes) == VK_NULL_HANDLE)
	{
		VkDescriptorSetLayout layout = GetDeviceObjectFactory().GetInlineConstantBufferLayout();
		pActualBuffer->AsDynamicOffsetBuffer()->CreateDynamicDescriptorSet(layout, bytes);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareVertexBuffersForUseImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams) const
{
	buffer_size_t offset;
	for (uint32 i = 0; i < numStreams; ++i)
	{
		const SStreamInfo stream = vertexStreams[i];
		CBufferResource* const pActualBuffer = gcpRendD3D.m_DevBufMan.GetD3D(stream.hStream, &offset);
		RequestTransition(pActualBuffer, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareIndexBufferForUseImpl(const CDeviceInputStream* indexStream) const
{
	buffer_size_t offset;
	const SStreamInfo stream = *indexStream;
	CBufferResource* const pActualBuffer = gcpRendD3D.m_DevBufMan.GetD3D(stream.hStream, &offset);
	RequestTransition(pActualBuffer, VK_ACCESS_INDEX_READ_BIT);
}

void CDeviceGraphicsCommandInterfaceImpl::BeginRenderPassImpl(const CDeviceRenderPass& renderPass, const D3DRectangle& renderArea)
{
	// clamp render limits to valid area. high level code sometimes provides wrong size.
	VkOffset2D renderAreaTopLeft = { renderArea.left, renderArea.top };
	VkExtent2D renderAreaSize = 
	{
		min(renderPass.m_frameBufferExtent.width,  uint32(renderArea.right - renderArea.left)),
		min(renderPass.m_frameBufferExtent.height, uint32(renderArea.bottom - renderArea.top))
	};
	
	VkRenderPassBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.renderPass = renderPass.m_renderPassHandle;
	beginInfo.framebuffer = renderPass.m_frameBufferHandle;
	beginInfo.renderArea.offset = renderAreaTopLeft;
	beginInfo.renderArea.extent = renderAreaSize; 
	beginInfo.clearValueCount = 0;
	beginInfo.pClearValues = nullptr;

	vkCmdBeginRenderPass(GetVKCommandList()->GetVkCommandList(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CDeviceGraphicsCommandInterfaceImpl::EndRenderPassImpl(const CDeviceRenderPass& renderPass)
{
	vkCmdEndRenderPass(GetVKCommandList()->GetVkCommandList());
}

void CDeviceGraphicsCommandInterfaceImpl::SetViewportsImpl(uint32 vpCount, const D3DViewPort* pViewports)
{
	vkCmdSetViewport(GetVKCommandList()->GetVkCommandList(), 0, vpCount, reinterpret_cast<const VkViewport*>(pViewports));
}

void CDeviceGraphicsCommandInterfaceImpl::SetScissorRectsImpl(uint32 rcCount, const D3DRectangle* pRects)
{
	vkCmdSetScissor(GetVKCommandList()->GetVkCommandList(), 0, rcCount, reinterpret_cast<const VkRect2D*>(pRects));
}

void CDeviceGraphicsCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceGraphicsPSO* pDevicePSO)
{
	const CDeviceGraphicsPSO_Vulkan* pVkDevicePSO = reinterpret_cast<const CDeviceGraphicsPSO_Vulkan*>(pDevicePSO);

	vkCmdBindPipeline(GetVKCommandList()->GetVkCommandList(), VK_PIPELINE_BIND_POINT_GRAPHICS, pVkDevicePSO->GetVkPipeline());
	m_computeState.pPipelineState = nullptr;
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourceLayoutImpl(const CDeviceResourceLayout* pResourceLayout)
{
	m_graphicsState.custom.pendingBindings.Reset();
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	const CDeviceResourceSet_Vulkan* pVkResources = reinterpret_cast<const CDeviceResourceSet_Vulkan*>(pResources);
	CRY_ASSERT(pVkResources->GetVKDescriptorSet() != VK_NULL_HANDLE);

	m_graphicsState.custom.pendingBindings.AppendDescriptorSet(bindSlot, pVkResources->GetVKDescriptorSet(), nullptr);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, EShaderStage_None);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_graphicsState.pResourceLayout.cachedValue);
	
	buffer_size_t offset, size;
	NCryVulkan::CBufferResource* pVkBuffer = pBuffer->GetD3D(&offset, &size);
	VK_ASSERT(pVkBuffer && pVkBuffer->GetHandle() && pVkBuffer->AsDynamicOffsetBuffer() != nullptr);
	
	VkDescriptorSet dynamicDescriptorSet = pVkBuffer->AsDynamicOffsetBuffer()->GetDynamicDescriptorSet(size);
	CRY_ASSERT(dynamicDescriptorSet != VK_NULL_HANDLE);

	m_graphicsState.custom.pendingBindings.AppendDescriptorSet(bindSlot, dynamicDescriptorSet, &offset);
}

void CDeviceGraphicsCommandInterfaceImpl::SetVertexBuffersImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams)
{
	VkBuffer buffers[8];
	VkDeviceSize offsets[8];

	for (int i = 0; i <= lastStreamSlot+1; ++i)
		buffers[i] = VK_NULL_HANDLE;

	for (uint32 i = 0; i < numStreams; i++)
	{
		const CDeviceInputStream& vertexStream = vertexStreams[i];
		const int curSlot = vertexStream.nSlot;

		CRY_ASSERT(vertexStream.hStream != ~0u);
		{
			buffer_size_t offset;
			auto* pBuffer = gcpRendD3D.m_DevBufMan.GetD3D(vertexStream.hStream, &offset);

			buffers[curSlot] = pBuffer->GetHandle();
			offsets[curSlot] = offset;
		}
	}

	// NOTE: assuming slot 0 is always used
	VK_ASSERT(buffers[0] != VK_NULL_HANDLE && "Invalid vertex buffer stream #0");
	for (int rangeStart = 0, rangeEnd = 1; rangeStart <= lastStreamSlot;  rangeStart = rangeEnd = rangeEnd + 1)
	{
		while (buffers[rangeEnd] != VK_NULL_HANDLE)
			++rangeEnd;

		if (rangeEnd > rangeStart)
		{
			vkCmdBindVertexBuffers(GetVKCommandList()->GetVkCommandList(), rangeStart, rangeEnd - rangeStart, buffers + rangeStart, offsets + rangeStart);
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetIndexBufferImpl(const CDeviceInputStream* indexStream)
{
	CRY_ASSERT(indexStream->hStream != ~0u);
	{
		buffer_size_t offset;
		auto* pBuffer = gcpRendD3D.m_DevBufMan.GetD3D(indexStream->hStream, &offset);
		VK_ASSERT(pBuffer && pBuffer->GetHandle() && "Invalid index buffer stream");

#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
		vkCmdBindIndexBuffer(GetVKCommandList()->GetVkCommandList(), pBuffer->GetHandle(), offset, VK_INDEX_TYPE_UINT16);
#else
		vkCmdBindIndexBuffer(GetVKCommandList()->GetVkCommandList(), pBuffer->GetHandle(), offset, indexStream->nStride == DXGI_FORMAT_R16_UINT ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
#endif
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetStencilRefImpl(uint8 stencilRefValue)
{
	vkCmdSetStencilReference(GetVKCommandList()->GetVkCommandList(), VK_STENCIL_FRONT_AND_BACK, stencilRefValue);
}

void CDeviceGraphicsCommandInterfaceImpl::SetDepthBiasImpl(float constBias, float slopeBias, float biasClamp)
{
	vkCmdSetDepthBias(GetVKCommandList()->GetVkCommandList(), constBias, biasClamp, slopeBias);
}

void CDeviceGraphicsCommandInterfaceImpl::DrawImpl(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_graphicsState.pResourceLayout.cachedValue);

	ApplyPendingBindings(GetVKCommandList()->GetVkCommandList(), pVkLayout->GetVkPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsState.custom.pendingBindings);
	m_graphicsState.custom.pendingBindings.Reset();

	vkCmdDraw(GetVKCommandList()->GetVkCommandList(), VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	GetVKCommandList()->m_nCommands += CLCOUNT_DRAW;
}

void CDeviceGraphicsCommandInterfaceImpl::DrawIndexedImpl(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_graphicsState.pResourceLayout.cachedValue);

	ApplyPendingBindings(GetVKCommandList()->GetVkCommandList(), pVkLayout->GetVkPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsState.custom.pendingBindings);
	m_graphicsState.custom.pendingBindings.Reset();

	vkCmdDrawIndexed(GetVKCommandList()->GetVkCommandList(), IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartIndexLocation);
	GetVKCommandList()->m_nCommands += CLCOUNT_DRAW;
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DSurface* pView, const FLOAT Color[4], UINT NumRects, const D3D11_RECT* pRects)
{
	CRY_ASSERT(NumRects == 0); // currently not supported on Vulkan

	CImageResource* pImage              = pView->GetResource();
	VK_ASSERT(pImage && pImage->GetFlag(NCryVulkan::kImageFlagColorAttachment) && "The image was not created as a color-target");
	VkImageSubresourceRange subresource = pView->GetSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

	VkClearColorValue clearValue;
	clearValue.float32[0] = Color[0];
	clearValue.float32[1] = Color[1];
	clearValue.float32[2] = Color[2];
	clearValue.float32[3] = Color[3];

	RequestTransition(pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
	GetVKCommandList()->PendingResourceBarriers();

	vkCmdClearColorImage(GetVKCommandList()->GetVkCommandList(), pImage->GetHandle(), pImage->GetLayout(), &clearValue, 1, &subresource);
	GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DDepthSurface* pView, int clearFlags, float depth, uint8 stencil, uint32 numRects, const D3D11_RECT* pRects)
{
	CRY_ASSERT(numRects == 0); // currently not supported on Vulkan

	VkImageAspectFlags aspectMask = 0;
	aspectMask |= (clearFlags & CLEAR_ZBUFFER) ? VK_IMAGE_ASPECT_DEPTH_BIT   : 0;
	aspectMask |= (clearFlags & CLEAR_STENCIL) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;

	CImageResource* pImage = pView->GetResource();
	VK_ASSERT(pImage && pImage->GetFlag(NCryVulkan::kImageFlagDepthAttachment) && "The image was not created as a depth-target");
	VkImageSubresourceRange subresource = pView->GetSubresourceRange(aspectMask);

	VkClearDepthStencilValue clearValue;
	clearValue.depth = depth;
	clearValue.stencil = stencil;

	RequestTransition(pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
	GetVKCommandList()->PendingResourceBarriers();

	vkCmdClearDepthStencilImage(GetVKCommandList()->GetVkCommandList(), pImage->GetHandle(), pImage->GetLayout(), &clearValue, 1, &subresource);
	GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
}

void CDeviceGraphicsCommandInterfaceImpl::BeginOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	pQuery->Begin(GetVKCommandList()->GetVkCommandList());
	GetVKCommandList()->m_nCommands += CLCOUNT_QUERY;
}

void CDeviceGraphicsCommandInterfaceImpl::EndOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	pQuery->End(GetVKCommandList()->GetVkCommandList());
	GetVKCommandList()->m_nCommands += CLCOUNT_QUERY;
}

void CDeviceGraphicsCommandInterfaceImpl::BindNullVertexBuffers()
{
	// bind null buffers to all potentially used slots so we don't get validation issues when binding streams with gaps
	VkBuffer nullBuffer = GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_BUFFER)->AsBuffer()->GetHandle();

	VkBuffer     buffers[VSF_NUM];
	VkDeviceSize offsets[VSF_NUM];

	for (int i = 0; i < CRY_ARRAY_COUNT(buffers); ++i)
	{
		buffers[i] = nullBuffer;
		offsets[i] = 0;
	}

	vkCmdBindVertexBuffers(GetVKCommandList()->GetVkCommandList(), 0, CRY_ARRAY_COUNT(buffers), buffers, offsets);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeviceComputeCommandInterfaceImpl::PrepareUAVsForUseImpl(uint32 viewCount, CGpuBuffer** pViews) const
{
	for (uint32 i = 0; i < viewCount; ++i)
	{
		RequestTransition(pViews[i]->GetDevBuffer()->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	}
}

void CDeviceComputeCommandInterfaceImpl::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	CDeviceResourceSet_Vulkan* const pResourceSet = static_cast<CDeviceResourceSet_Vulkan*>(pResources);
	auto lambda = [this](CMemoryResource* pResource, bool bWritable)
	{
		if (CDynamicOffsetBufferResource* pBuffer = pResource->AsDynamicOffsetBuffer())
		{
			RequestTransition(pBuffer, VK_ACCESS_UNIFORM_READ_BIT);
		}
		else
		{
			const VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | (bWritable ? VK_ACCESS_SHADER_WRITE_BIT : 0);
			const VkImageLayout layout = bWritable ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			if (CBufferResource* pBuffer = pResource->AsBuffer())
			{
				RequestTransition(pBuffer, access);
			}
			else if (CImageResource* pImage = pResource->AsImage())
			{
				RequestTransition(pImage, layout, access);
			}
		}
	};
	pResourceSet->EnumerateInUseResources(lambda);
}

void CDeviceComputeCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlots, EShaderStage shaderStages) const
{
	buffer_size_t offset, bytes;
	CBufferResource* const pActualBuffer = pBuffer->GetD3D(&offset, &bytes);
	RequestTransition(pActualBuffer, VK_ACCESS_UNIFORM_READ_BIT);
	CRY_ASSERT(pActualBuffer->AsDynamicOffsetBuffer() != nullptr);

	if (pActualBuffer->AsDynamicOffsetBuffer()->GetDynamicDescriptorSet(bytes) == VK_NULL_HANDLE)
	{
		VkDescriptorSetLayout layout = GetDeviceObjectFactory().GetInlineConstantBufferLayout();
		pActualBuffer->AsDynamicOffsetBuffer()->CreateDynamicDescriptorSet(layout, bytes);
	}
}

void CDeviceComputeCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	const CDeviceResourceSet_Vulkan* pVkResources = reinterpret_cast<const CDeviceResourceSet_Vulkan*>(pResources);
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_computeState.pResourceLayout.cachedValue);

	VkDescriptorSet descriptorSetHandle = pVkResources->GetVKDescriptorSet();
	CRY_ASSERT(descriptorSetHandle != VK_NULL_HANDLE);

	m_computeState.custom.pendingBindings.AppendDescriptorSet(bindSlot, descriptorSetHandle, nullptr);
}

void CDeviceComputeCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_computeState.pResourceLayout.cachedValue);

	buffer_size_t offset, size;
	NCryVulkan::CBufferResource* pVkBuffer = pBuffer->GetD3D(&offset, &size);
	CRY_ASSERT(pVkBuffer->AsDynamicOffsetBuffer() != nullptr); // Needs to be a buffer with dynamic offsets

	VkDescriptorSet dynamicDescriptorSet = pVkBuffer->AsDynamicOffsetBuffer()->GetDynamicDescriptorSet(size);
	CRY_ASSERT(dynamicDescriptorSet != VK_NULL_HANDLE);

	m_computeState.custom.pendingBindings.AppendDescriptorSet(bindSlot, dynamicDescriptorSet, &offset);
}

void CDeviceComputeCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceComputePSO* pDevicePSO)
{
	const CDeviceComputePSO_Vulkan* pVkDevicePSO = reinterpret_cast<const CDeviceComputePSO_Vulkan*>(pDevicePSO);

	vkCmdBindPipeline(GetVKCommandList()->GetVkCommandList(), VK_PIPELINE_BIND_POINT_COMPUTE, pVkDevicePSO->GetVkPipeline());
	m_graphicsState.pPipelineState = nullptr;
}

void CDeviceComputeCommandInterfaceImpl::SetResourceLayoutImpl(const CDeviceResourceLayout* pResourceLayout)
{
	m_computeState.custom.pendingBindings.Reset();
}

void CDeviceComputeCommandInterfaceImpl::DispatchImpl(uint32 X, uint32 Y, uint32 Z)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_computeState.pResourceLayout.cachedValue);

	ApplyPendingBindings(GetVKCommandList()->GetVkCommandList(), pVkLayout->GetVkPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, m_computeState.custom.pendingBindings);
	m_computeState.custom.pendingBindings.Reset();

	GetVKCommandList()->PendingResourceBarriers();
	vkCmdDispatch(GetVKCommandList()->GetVkCommandList(), X, Y, Z);
	GetVKCommandList()->m_nCommands += CLCOUNT_DISPATCH;
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	VK_ASSERT(NumRects == 0 && "Clear area not supported (yet)");
	VK_ASSERT(pView->GetResource()->GetFlag(kResourceFlagCopyWritable) && "Resource does not have required capabilities");

	CImageResource* pImage = pView->GetResource()->AsImage();
	if (pImage)
	{
		CImageView* pImageView = static_cast<CImageView*>(pView);
		VkImageSubresourceRange subresource = pImageView->GetSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

		VkClearColorValue clearValue;
		clearValue.float32[0] = Values[0];
		clearValue.float32[1] = Values[1];
		clearValue.float32[2] = Values[2];
		clearValue.float32[3] = Values[3];

		RequestTransition(pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
		GetVKCommandList()->PendingResourceBarriers();

		vkCmdClearColorImage(GetVKCommandList()->GetVkCommandList(), pImage->GetHandle(), pImage->GetLayout(), &clearValue, 1, &subresource);
		GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
	}
	else
	{
		CBufferResource* pBuffer = pView->GetResource()->AsBuffer();
		CBufferView* pBufferView = static_cast<CBufferView*>(pView);

		VK_ASSERT(Values[0] == Values[1] && Values[0] == Values[2] && Values[0] == Values[3] && "Can only fill with 32 bit data");
		VK_ASSERT(pBuffer && "Unknown resource type");

		VkDescriptorBufferInfo info;
		pBufferView->FillInfo(info);

		RequestTransition(pBuffer, VK_ACCESS_TRANSFER_WRITE_BIT);
		GetVKCommandList()->PendingResourceBarriers();

		vkCmdFillBuffer(GetVKCommandList()->GetVkCommandList(), info.buffer, info.offset, info.range, reinterpret_cast<const uint32*>(Values)[0]);
		GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
	}
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	VK_ASSERT(NumRects == 0 && "Clear area not supported (yet)");
	VK_ASSERT(pView->GetResource()->GetFlag(kResourceFlagCopyWritable) && "Resource does not have required capabilities");

	CImageResource* pImage = pView->GetResource()->AsImage();
	if (pImage)
	{
		CImageView* pImageView = static_cast<CImageView*>(pView);
		VkImageSubresourceRange subresource = pImageView->GetSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

		VkClearColorValue clearValue;
		clearValue.uint32[0] = Values[0];
		clearValue.uint32[1] = Values[1];
		clearValue.uint32[2] = Values[2];
		clearValue.uint32[3] = Values[3];

		RequestTransition(pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
		GetVKCommandList()->PendingResourceBarriers();

		vkCmdClearColorImage(GetVKCommandList()->GetVkCommandList(), pImage->GetHandle(), pImage->GetLayout(), &clearValue, 1, &subresource);
		GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
	}
	else
	{
		CBufferResource* pBuffer = pView->GetResource()->AsBuffer();
		CBufferView* pBufferView = static_cast<CBufferView*>(pView);

		VK_ASSERT(Values[0] == Values[1] && Values[0] == Values[2] && Values[0] == Values[3] && "Can only fill with 32 bit data");
		VK_ASSERT(pBuffer && "Unknown resource type");

		VkDescriptorBufferInfo info;
		pBufferView->FillInfo(info);

		RequestTransition(pBuffer, VK_ACCESS_TRANSFER_WRITE_BIT);
		GetVKCommandList()->PendingResourceBarriers();

		vkCmdFillBuffer(GetVKCommandList()->GetVkCommandList(), info.buffer, info.offset, info.range, Values[0]);
		GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static VkImageAspectFlags GetCopyableAspects(CImageResource* pImage)
{
	VkImageAspectFlags result = 0;
	result |= pImage->GetFlag(kImageFlagDepthAttachment) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	result |= pImage->GetFlag(kImageFlagStencilAttachment) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
	result |= result == 0 ? VK_IMAGE_ASPECT_COLOR_BIT : 0;
	return result;
}

static bool GetBlockSize(VkFormat format, uint32_t& blockWidth, uint32_t& blockHeight, uint32_t& blockBytes)
{
	if (VK_FORMAT_BC1_RGB_UNORM_BLOCK <= format && format <= VK_FORMAT_ASTC_4x4_SRGB_BLOCK)
	{
		// Algorithmically select from table: 0000 1111 0011 1111 0000 1100 11...11 (into lowest bit)
		// Attribute (base >> 2)            : 0000 1111 0000 1111 0000 1111
		// Attribute (base > 21)            : 0000 0000 0000 0000 0000 0011
		// Attribute ((base & 30) == 10)    : 0000 0000 0011 0000 0000 0000
		const uint32_t base = format - VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		const uint32_t pickBit = (base >> 2) ^ (base > 21) | ((base & 30) == 10) | (base > 23);
		blockWidth = 4;
		blockHeight = 4;
		blockBytes = 8 << (pickBit & 1);
		return true;
	}
	else if (VK_FORMAT_ASTC_5x4_UNORM_BLOCK <= format && format <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
	{
		static const uint8_t kBlockSizeTable[] =
		{
			(5 << 4) | 4,   // 5x4
			(5 << 4) | 5,   // 5x5
			(6 << 4) | 5,   // 6x5
			(6 << 4) | 6,   // 6x6
			(8 << 4) | 5,   // 8x5
			(8 << 4) | 6,   // 8x6
			(8 << 4) | 8,   // 8x8
			(10 << 4) | 5,  // 10x5
			(10 << 4) | 6,  // 10x6
			(10 << 4) | 8,  // 10x8
			(10 << 4) | 10, // 10x10
			(12 << 4) | 10, // 12x10
			(12 << 4) | 12, // 12x12
		};
		const uint32_t base = format - VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
		const uint8_t packed = kBlockSizeTable[base >> 1];
		blockWidth = packed >> 4;
		blockHeight = packed & 0xF;
		blockBytes = 16;
		return true;
	}

	return false;
}

void CDeviceCopyCommandInterfaceImpl::CopyBuffer(CBufferResource* pSrc, CBufferResource* pDst, const SResourceRegionMapping& mapping)
{
	const uint32_t srcSize = pSrc->GetStride() * pSrc->GetElementCount();
	const uint32_t dstSize = pDst->GetStride() * pDst->GetElementCount();
	VK_ASSERT(pSrc->GetStride() == pDst->GetStride() && "Buffer element sizes must be compatible"); // Not actually a Vk requirement, but likely a bug
	VK_ASSERT(mapping.Extent.Width > 0 && "Invalid extents");
	VK_ASSERT(mapping.SourceOffset.Left + mapping.Extent.Width <= srcSize && "Source region too large");
	VK_ASSERT(mapping.DestinationOffset.Left + mapping.Extent.Width <= dstSize && "Destination region too large");
	VK_ASSERT(mapping.SourceOffset.Subresource == 0 && mapping.DestinationOffset.Subresource == 0 && mapping.Extent.Subresources == 1 && "Buffer array slices not yet supported");

	if (pSrc == pDst)
	{
		RequestTransition(pSrc, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	else
	{
		RequestTransition(pSrc, VK_ACCESS_TRANSFER_READ_BIT);
		RequestTransition(pDst, VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	GetVKCommandList()->PendingResourceBarriers();

	VkBufferCopy copy;
	copy.srcOffset = mapping.SourceOffset.Left;
	copy.dstOffset = mapping.DestinationOffset.Left;
	copy.size = mapping.Extent.Width;
	vkCmdCopyBuffer(GetVKCommandList()->GetVkCommandList(), pSrc->GetHandle(), pDst->GetHandle(), 1, &copy);
	GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
}

void CDeviceCopyCommandInterfaceImpl::CopyImage(CImageResource* pSrc, CImageResource* pDst, const SResourceRegionMapping& mapping)
{
	const uint32_t srcMips = pSrc->GetMipCount();
	const uint32_t srcFirstMip = mapping.SourceOffset.Subresource % srcMips;
	const uint32_t srcFirstSlice = mapping.SourceOffset.Subresource / srcMips;
	const uint32_t srcLastMip = (mapping.SourceOffset.Subresource + mapping.Extent.Subresources - 1) % srcMips;
	const uint32_t srcLastSlice = (mapping.SourceOffset.Subresource + mapping.Extent.Subresources - 1) / srcMips;
	const uint32_t numMips = srcLastMip - srcFirstMip + 1;
	const uint32_t numSlices = srcLastSlice - srcFirstSlice + 1;
	VK_ASSERT(mapping.Extent.Width * mapping.Extent.Height * mapping.Extent.Depth * mapping.Extent.Subresources > 0 && "Invalid extents");
	VK_ASSERT(numMips * numSlices == mapping.Extent.Subresources && "Partial copies that cross slice must start at mip 0, and request whole mip chains");
	VK_ASSERT(srcFirstMip + numMips <= srcMips && srcFirstSlice + numSlices <= pSrc->GetSliceCount() && "Source subresource out of bounds");

	const uint32_t dstMips = pDst->GetMipCount();
	const uint32_t dstFirstMip = mapping.DestinationOffset.Subresource % dstMips;
	const uint32_t dstFirstSlice = mapping.DestinationOffset.Subresource / dstMips;
	VK_ASSERT(dstFirstMip + numMips <= dstMips && dstFirstSlice + numSlices <= pDst->GetSliceCount() && "Destination subresource out of bounds");

	const VkImageAspectFlags srcAspects = GetCopyableAspects(pSrc);
	const VkImageAspectFlags dstAspects = GetCopyableAspects(pDst);

	uint32_t mipIndex = 0;
	uint32_t srcWidth = mapping.Extent.Width;
	uint32_t srcHeight = mapping.Extent.Height;
	uint32_t srcDepth = mapping.Extent.Depth;
	uint32_t srcX = mapping.SourceOffset.Left;
	uint32_t srcY = mapping.SourceOffset.Top;
	uint32_t srcZ = mapping.SourceOffset.Front;
	uint32_t dstWidth = srcWidth;
	uint32_t dstHeight = srcHeight;
	uint32_t dstDepth = srcDepth;
	uint32_t dstX = mapping.DestinationOffset.Left;
	uint32_t dstY = mapping.DestinationOffset.Top;
	uint32_t dstZ = mapping.DestinationOffset.Front;

	uint32_t srcBlockWidth = 1, srcBlockHeight = 1, srcBlockBytes;
	uint32_t dstBlockWidth = 1, dstBlockHeight = 1, dstBlockBytes;
	const bool bSrcIsBlocks = GetBlockSize(pSrc->GetFormat(), srcBlockWidth, srcBlockHeight, srcBlockBytes);
	const bool bDstIsBlocks = GetBlockSize(pDst->GetFormat(), dstBlockWidth, dstBlockHeight, dstBlockBytes);
	VK_ASSERT(!(bSrcIsBlocks && bDstIsBlocks) || (srcBlockWidth == dstBlockWidth && srcBlockHeight == dstBlockHeight && srcBlockBytes == dstBlockBytes) && "Incompatible block compression");

	// Round up extents to block size, so we can do shifts on them in the loop.
	// Unlike extents, the input coordinates MUST always align to block size for all mips.
	// Note: ASTC block sizes are not necessary power-of-two, so we have to actually do modulo/division here :(
	if (bSrcIsBlocks)
	{
		srcWidth = (srcWidth + srcBlockWidth - 1) / srcBlockWidth * srcBlockWidth;
		srcHeight = (srcHeight + srcBlockHeight - 1) / srcBlockHeight * srcBlockHeight;
	}
	if (bDstIsBlocks)
	{
		dstWidth = (dstWidth + dstBlockWidth - 1) / dstBlockWidth * dstBlockWidth;
		dstHeight = (dstHeight + dstBlockHeight - 1) / dstBlockHeight * dstBlockHeight;
	}
	if (bSrcIsBlocks && !bDstIsBlocks)
	{
		dstWidth /= srcBlockWidth;
		dstHeight /= srcBlockHeight;
	}
	else if (!bSrcIsBlocks && bDstIsBlocks)
	{
		dstWidth *= srcBlockWidth;
		dstHeight *= srcBlockHeight;
	}
	VK_ASSERT(srcX % (srcBlockWidth << (numMips - 1)) == 0 && "Source offset X not aligned for block-mip access");
	VK_ASSERT(srcY % (srcBlockHeight << (numMips - 1)) == 0 && "Source offset Y not aligned for block-mip access");
	VK_ASSERT(dstX % (dstBlockWidth << (numMips - 1)) == 0 && "Destination offset X not aligned for block-mip access");
	VK_ASSERT(dstY % (dstBlockHeight << (numMips - 1)) == 0 && "Destination offset Y not aligned for block-mip access");
	VK_ASSERT(srcWidth % (srcBlockWidth << (numMips - 1)) == 0 && "Source width is not aligned for block-mip access");
	VK_ASSERT(srcHeight % (srcBlockHeight << (numMips - 1)) == 0 && "Source height is not aligned for block-mip access");
	VK_ASSERT(dstWidth % (dstBlockWidth << (numMips - 1)) == 0 && "Destination width is not aligned for block-mip access");
	VK_ASSERT(dstHeight % (dstBlockHeight << (numMips - 1)) == 0 && "Destination height is not aligned for block-mip access");

	const uint32_t srcMipWidth = std::max(pSrc->GetWidth() >> srcFirstMip, srcBlockWidth);
	const uint32_t srcMipHeight = std::max(pSrc->GetHeight() >> srcFirstMip, srcBlockHeight);
	const uint32_t srcMipDepth = std::max(pSrc->GetDepth() >> srcFirstMip, 1U);
	const uint32_t dstMipWidth = std::max(pDst->GetWidth() >> dstFirstMip, dstBlockWidth);
	const uint32_t dstMipHeight = std::max(pDst->GetHeight() >> dstFirstMip, dstBlockHeight);
	const uint32_t dstMipDepth = std::max(pDst->GetDepth() >> dstFirstMip, 1U);
	VK_ASSERT(srcX + srcWidth <= srcMipWidth && srcY + srcHeight <= srcMipHeight && srcZ + srcDepth <= srcMipDepth && "Source region too large");
	VK_ASSERT(dstX + dstWidth <= dstMipWidth && dstY + dstHeight <= dstMipHeight && dstZ + dstDepth <= dstMipDepth && "Destination region too large");

	const uint32_t kBatchSize = 16;
	VkImageCopy batch[kBatchSize];
	VK_ASSERT(numMips <= kBatchSize && "Cannot copy more than 16 mips at a time");
	for (uint32_t srcMip = srcFirstMip; srcMip <= srcLastMip; ++srcMip, ++mipIndex)
	{
		batch[mipIndex].srcSubresource.aspectMask = srcAspects;
		batch[mipIndex].srcSubresource.mipLevel = srcMip;
		batch[mipIndex].srcSubresource.baseArrayLayer = srcFirstSlice;
		batch[mipIndex].srcSubresource.layerCount = numSlices;
		batch[mipIndex].srcOffset.x = srcX;
		batch[mipIndex].srcOffset.y = srcY;
		batch[mipIndex].srcOffset.z = srcZ;
		batch[mipIndex].dstSubresource.aspectMask = srcAspects;
		batch[mipIndex].dstSubresource.mipLevel = dstFirstMip + mipIndex;
		batch[mipIndex].dstSubresource.baseArrayLayer = dstFirstSlice;
		batch[mipIndex].dstSubresource.layerCount = numSlices;
		batch[mipIndex].dstOffset.x = dstX;
		batch[mipIndex].dstOffset.y = dstY;
		batch[mipIndex].dstOffset.z = dstZ;
		batch[mipIndex].extent.width = srcWidth;
		batch[mipIndex].extent.height = srcHeight;
		batch[mipIndex].extent.depth = srcDepth;

		srcWidth = std::max(srcWidth >> 1, srcBlockWidth);
		srcHeight = std::max(srcHeight >> 1, srcBlockHeight);
		srcDepth = std::max(srcDepth >> 1, 1U);
		srcX >>= 1;
		srcY >>= 1;
		srcZ >>= 1;
		dstX >>= 1;
		dstY >>= 1;
		dstZ >>= 1;
	}

	if (pSrc == pDst)
	{
		RequestTransition(pSrc, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	else
	{
		RequestTransition(pSrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
		RequestTransition(pDst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	GetVKCommandList()->PendingResourceBarriers();

	vkCmdCopyImage(GetVKCommandList()->GetVkCommandList(), pSrc->GetHandle(), pSrc->GetLayout(), pDst->GetHandle(), pDst->GetLayout(), mipIndex, batch);
	GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
}

bool CDeviceCopyCommandInterfaceImpl::FillBuffer(const void* pSrc, CBufferResource* pDst, const SResourceMemoryMapping& mapping)
{
	const uint32_t srcBytes = mapping.MemoryLayout.volumeStride;
	const uint32_t dstOffset = mapping.ResourceOffset.Left;
	const uint32_t dstBytes = mapping.Extent.Width;

	VK_ASSERT(pDst->GetFlag(kResourceFlagCpuWritable) && "Destination buffer must be mappable for the CPU");
	VK_ASSERT(mapping.ResourceOffset.Left + mapping.Extent.Width <= pDst->GetSize() && "Invalid target region");
	VK_ASSERT(mapping.ResourceOffset.Subresource == 0 && "Buffer array slices not yet supported");
	VK_ASSERT(srcBytes <= dstBytes && "Source data does not fit into target region");

	uint8_t* pMapped = static_cast<uint8_t*>(pDst->Map());
	if (pMapped)
	{
		::memcpy(pMapped + dstOffset, pSrc, srcBytes); // Non-temporal hint?
		pDst->Unmap();
	}
	return pMapped != nullptr;
}

CBufferResource* CDeviceCopyCommandInterfaceImpl::UploadBuffer(const void* pSrc, CBufferResource* pDst, const SResourceMemoryMapping& mapping, bool bAllowGpu)
{
	const uint32_t srcBytes = mapping.MemoryLayout.volumeStride;
	const uint32_t dstOffset = mapping.ResourceOffset.Left;
	VK_ASSERT(mapping.MemoryLayout.rowStride == mapping.MemoryLayout.volumeStride && "Invalid source memory layout"); // Fill uses volumeStride since it also fills images.
	VK_ASSERT(mapping.ResourceOffset.Subresource == 0 && "Buffer array slices not yet supported");

	const bool bIsUsedByGpu = false; // TODO: Implement this

	CBufferResource* pStaging = pDst;
	const SResourceMemoryMapping* pMapping = &mapping;
	SResourceMemoryMapping tempMapping;

	const bool bStage = !pDst->GetFlag(kResourceFlagCpuWritable) || bIsUsedByGpu;
	if (bStage)
	{
		// When we are not updating at the start of the buffer, we need to adjust the fill parameters,
		// since the staging buffer is not necessarily the same size as the destination buffer.
		if (mapping.ResourceOffset.Left != 0)
		{
			tempMapping = mapping;
			tempMapping.ResourceOffset.Left = 0;
			pMapping = &tempMapping;
		}

		if (GetDevice()->CreateOrReuseStagingResource(pDst, srcBytes, &pStaging, true) != VK_SUCCESS)
		{
			VK_ASSERT(false && "Upload buffer skipped: Cannot create staging buffer");
			return nullptr;
		}
	}

	const bool bFilled = FillBuffer(pSrc, pStaging, *pMapping);
	VK_ASSERT(bFilled && "Unable to fill staging buffer");

	if (bStage)
	{
		if (bFilled)
		{
			if (!bAllowGpu)
			{
				// We would need to use the GPU to perform the upload, however the caller chose not to allow this.
				// Instead hand-off the staging buffer containing the data to the caller to handle in some other way.
				VK_ASSERT(srcBytes % pDst->GetStride() == 0 && dstOffset % pDst->GetStride() == 0);
				pStaging->SetStrideAndElementCount(pDst->GetStride(), srcBytes / pDst->GetStride());
				return pStaging;
			}

			RequestTransition(pStaging, VK_ACCESS_TRANSFER_READ_BIT);
			RequestTransition(pDst, VK_ACCESS_TRANSFER_WRITE_BIT);
			GetVKCommandList()->PendingResourceBarriers();

			VkBufferCopy info;
			info.srcOffset = 0;
			info.dstOffset = dstOffset;
			info.size = srcBytes;
			vkCmdCopyBuffer(GetVKCommandList()->GetVkCommandList(), pStaging->GetHandle(), pDst->GetHandle(), 1, &info);
			GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
		}

		pStaging->Release(); // trigger ReleaseLater with kResourceFlagReusable
	}

	return nullptr;
}

void CDeviceCopyCommandInterfaceImpl::UploadImage(const void* pSrc, CImageResource* pDst, const SResourceMemoryMapping& mapping, bool bExt)
{
	// For now, always stage image uploads, because all images should be tiled anyway, so direct CPU write should never be set in general.
	// However, Vk does not forbid direct CPU write to images entirely, so we may need to implement a FillImage + direct write at some future point.
	VK_ASSERT(mapping.MemoryLayout.rowStride % mapping.MemoryLayout.typeStride == 0 && "Texel rows must be aligned to the texel size");
	VK_ASSERT(mapping.MemoryLayout.planeStride % mapping.MemoryLayout.rowStride == 0 && "Texel planes must be aligned to the texel rows");
	VK_ASSERT(mapping.MemoryLayout.volumeStride % mapping.MemoryLayout.planeStride == 0 && "Texel volumes must be aligned to the texel planes");

	CBufferResource* pStaging = nullptr;
	const uint32 srcBytes = mapping.MemoryLayout.volumeStride;
	if (GetDevice()->CreateOrReuseStagingResource(pDst, srcBytes, &pStaging, true) != VK_SUCCESS)
	{
		VK_ASSERT(false && "Upload buffer skipped: Cannot create staging buffer");
		return;
	}

	SResourceMemoryMapping bufferMapping;

	bufferMapping.MemoryLayout = mapping.MemoryLayout;
	bufferMapping.ResourceOffset.Left = 0;
	bufferMapping.ResourceOffset.Subresource = 0;
	bufferMapping.Extent.Width = srcBytes;
	bufferMapping.Extent.Subresources = 1;

	if (FillBuffer(pSrc, pStaging, bufferMapping))
	{
		UploadImage(pStaging, pDst, mapping, bExt);
	}

	pStaging->Release(); // trigger ReleaseLater with kResourceFlagReusable
}


void CDeviceCopyCommandInterfaceImpl::UploadImage(CBufferResource* pSrc, CImageResource* pDst, const SResourceMemoryMapping& mapping, bool bExt)
{
	VK_ASSERT(mapping.MemoryLayout.rowStride % mapping.MemoryLayout.typeStride == 0 && "Texel rows must be aligned to the texel size");
	VK_ASSERT(mapping.MemoryLayout.planeStride % mapping.MemoryLayout.rowStride == 0 && "Texel planes must be aligned to the texel rows");
	VK_ASSERT(mapping.MemoryLayout.volumeStride % mapping.MemoryLayout.planeStride == 0 && "Texel volumes must be aligned to the texel planes");

	RequestTransition(pDst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
	RequestTransition(pSrc, VK_ACCESS_TRANSFER_READ_BIT);
	GetVKCommandList()->PendingResourceBarriers();

	const uint32_t dstMips = pDst->GetMipCount();
	const uint32_t dstFirstMip = mapping.ResourceOffset.Subresource % dstMips;
	const uint32_t dstFirstSlice = mapping.ResourceOffset.Subresource / dstMips;
	const uint32_t dstLastMip = (mapping.ResourceOffset.Subresource + mapping.Extent.Subresources - 1) % dstMips;
	const uint32_t dstLastSlice = (mapping.ResourceOffset.Subresource + mapping.Extent.Subresources - 1) / dstMips;
	const uint32_t numMips = dstLastMip - dstFirstMip + 1;
	const uint32_t numSlices = dstLastSlice - dstFirstSlice + 1;
	VK_ASSERT((bExt || numMips * numSlices == mapping.Extent.Subresources) && "Partial copies that cross slice must start at mip 0, and request whole mip chains");
	VK_ASSERT(dstFirstMip + numMips <= dstMips && dstFirstSlice + numSlices <= pDst->GetSliceCount() && "Destination subresource out of bounds");

	uint32_t blockWidth = 1, blockHeight = 1, blockBytes = mapping.MemoryLayout.typeStride;
	GetBlockSize(pDst->GetFormat(), blockWidth, blockHeight, blockBytes);

	// This is a limitation of high-level, may eventually change.
	// When it does, we need to loop over the mips and fill one struct per iteration.
	// However, we can still batch to one vkCmdCopyBufferToImage
	VK_ASSERT(numMips == 1 && "Only 1 mip at a time expected");

	VkBufferImageCopy batch;
	batch.bufferOffset = 0;
	batch.bufferRowLength = mapping.MemoryLayout.rowStride / mapping.MemoryLayout.typeStride * blockWidth;
	batch.bufferImageHeight = mapping.MemoryLayout.planeStride / mapping.MemoryLayout.rowStride * blockHeight;
	batch.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	batch.imageSubresource.mipLevel = dstFirstMip;
	batch.imageSubresource.baseArrayLayer = dstFirstSlice;
	batch.imageSubresource.layerCount = numSlices;
	batch.imageOffset.x = mapping.ResourceOffset.Left;
	batch.imageOffset.y = mapping.ResourceOffset.Top;
	batch.imageOffset.z = mapping.ResourceOffset.Front;
	batch.imageExtent.width = mapping.Extent.Width;
	batch.imageExtent.height = mapping.Extent.Height;
	batch.imageExtent.depth = mapping.Extent.Depth;
	VK_ASSERT(batch.bufferRowLength >= batch.imageExtent.width && "Bad row-stride specified");
	VK_ASSERT(batch.bufferImageHeight >= batch.imageExtent.height && "Bad plane-stride specified");
	vkCmdCopyBufferToImage(GetVKCommandList()->GetVkCommandList(), pSrc->GetHandle(), pDst->GetHandle(), pDst->GetLayout(), 1, &batch);
	GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
}

void CDeviceCopyCommandInterfaceImpl::DownloadImage(NCryVulkan::CImageResource* pSrc, NCryVulkan::CBufferResource* pDst, const SResourceMemoryMapping& mapping, bool bExt)
{
	VK_ASSERT(mapping.MemoryLayout.rowStride % mapping.MemoryLayout.typeStride == 0 && "Texel rows must be aligned to the texel size");
	VK_ASSERT(mapping.MemoryLayout.planeStride % mapping.MemoryLayout.rowStride == 0 && "Texel planes must be aligned to the texel rows");
	VK_ASSERT(mapping.MemoryLayout.volumeStride % mapping.MemoryLayout.planeStride == 0 && "Texel volumes must be aligned to the texel planes");

	RequestTransition(pDst, VK_ACCESS_TRANSFER_WRITE_BIT);
	RequestTransition(pSrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
	GetVKCommandList()->PendingResourceBarriers();

	const uint32_t srcMips = pSrc->GetMipCount();
	const uint32_t srcFirstMip = mapping.ResourceOffset.Subresource % srcMips;
	const uint32_t srcFirstSlice = mapping.ResourceOffset.Subresource / srcMips;
	const uint32_t srcLastMip = (mapping.ResourceOffset.Subresource + mapping.Extent.Subresources - 1) % srcMips;
	const uint32_t srcLastSlice = (mapping.ResourceOffset.Subresource + mapping.Extent.Subresources - 1) / srcMips;
	const uint32_t numMips = srcLastMip - srcFirstMip + 1;
	const uint32_t numSlices = srcLastSlice - srcFirstSlice + 1;
	VK_ASSERT((bExt || numMips * numSlices == mapping.Extent.Subresources) && "Partial copies that cross slice must start at mip 0, and request whole mip chains");
	VK_ASSERT(srcFirstMip + numMips <= srcMips && srcFirstSlice + numSlices <= pSrc->GetSliceCount() && "Destination subresource out of bounds");

	uint32_t blockWidth = 1, blockHeight = 1, blockBytes = mapping.MemoryLayout.typeStride;
	GetBlockSize(pSrc->GetFormat(), blockWidth, blockHeight, blockBytes);

	// This is a limitation of high-level, may eventually change.
	// When it does, we need to loop over the mips and fill one struct per iteration.
	// However, we can still batch to one vkCmdCopyImageToBuffer
	VK_ASSERT(numMips == 1 && "Only 1 mip at a time expected");

	VkBufferImageCopy batch;
	batch.bufferOffset = 0;
	batch.bufferRowLength = mapping.MemoryLayout.rowStride / mapping.MemoryLayout.typeStride * blockWidth;
	batch.bufferImageHeight = mapping.MemoryLayout.planeStride / mapping.MemoryLayout.rowStride * blockHeight;
	batch.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	batch.imageSubresource.mipLevel = srcFirstMip;
	batch.imageSubresource.baseArrayLayer = srcFirstSlice;
	batch.imageSubresource.layerCount = numSlices;
	batch.imageOffset.x = mapping.ResourceOffset.Left;
	batch.imageOffset.y = mapping.ResourceOffset.Top;
	batch.imageOffset.z = mapping.ResourceOffset.Front;
	batch.imageExtent.width = mapping.Extent.Width;
	batch.imageExtent.height = mapping.Extent.Height;
	batch.imageExtent.depth = mapping.Extent.Depth;
	VK_ASSERT(batch.bufferRowLength >= batch.imageExtent.width && "Bad row-stride specified");
	VK_ASSERT(batch.bufferImageHeight >= batch.imageExtent.height && "Bad plane-stride specified");
	vkCmdCopyImageToBuffer(GetVKCommandList()->GetVkCommandList(), pSrc->GetHandle(), pSrc->GetLayout(), pDst->GetHandle(), 1, &batch);
	GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
}

////////////////////////////////////////////////////////////////////////////
CDeviceRenderPass::CDeviceRenderPass()
	: m_renderPassHandle(VK_NULL_HANDLE)
	, m_frameBufferHandle(VK_NULL_HANDLE)
{
	m_frameBufferExtent.width = 0;
	m_frameBufferExtent.height = 0;
}

CDeviceRenderPass::~CDeviceRenderPass()
{
	GetDevice()->DeferDestruction(m_renderPassHandle, m_frameBufferHandle);
}

bool CDeviceRenderPass::UpdateImpl(const CDeviceRenderPassDesc& passDesc)
{
	// render pass handle cannot change as PSOs would become invalid
	if (m_renderPassHandle == VK_NULL_HANDLE)
		InitVkRenderPass(passDesc);

	InitVkFrameBuffer(passDesc);

	return m_renderPassHandle != VK_NULL_HANDLE && m_frameBufferHandle != VK_NULL_HANDLE;
}

bool CDeviceRenderPass::InitVkRenderPass(const CDeviceRenderPassDesc& passDesc)
{
	int attachmentCount = 0;
	int colorAttachmentCount = 0;
	VkAttachmentDescription attachments[CDeviceRenderPassDesc::MaxRendertargetCount + 1];
	VkAttachmentReference colorAttachments[CDeviceRenderPassDesc::MaxRendertargetCount];
	VkAttachmentReference depthStencilAttachment;

	// NOTE: the following code deliberately falls back to CTexture::GetDstFormat as we cannot guarantee
	// the existence of device texture at this point.

	const auto& renderTargets = passDesc.GetRenderTargets();
	const auto& depthTarget = passDesc.GetDepthTarget();

	for (int i=0; i<renderTargets.size(); ++i)
	{
		if (!renderTargets[i].pTexture)
			break;

		DXGI_FORMAT deviceFormat = renderTargets[i].GetResourceFormat();

		attachments[attachmentCount].flags = 0;
		attachments[attachmentCount].format = DXGIFormatToVKFormat(deviceFormat);
		attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorAttachments[attachmentCount].attachment = i;
		colorAttachments[attachmentCount].layout = attachments[attachmentCount].initialLayout;

		++attachmentCount;
		++colorAttachmentCount;
	}

	if (depthTarget.pTexture)
	{
		DXGI_FORMAT deviceFormat = depthTarget.GetResourceFormat();


		attachments[attachmentCount].flags = 0;
		attachments[attachmentCount].format = DXGIFormatToVKFormat(deviceFormat);
		attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthStencilAttachment.attachment = attachmentCount;
		depthStencilAttachment.layout = attachments[attachmentCount].initialLayout;

		++attachmentCount;
	}

	VkSubpassDescription subpass;
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = nullptr;
	subpass.colorAttachmentCount = colorAttachmentCount;
	subpass.pColorAttachments = colorAttachments;
	subpass.pResolveAttachments = nullptr;
	subpass.pDepthStencilAttachment = depthTarget.pTexture ? &depthStencilAttachment : nullptr;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = nullptr;

	VkRenderPassCreateInfo renderPassCreateInfo;
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pNext = nullptr;
	renderPassCreateInfo.flags = 0;
	renderPassCreateInfo.pAttachments = attachments;
	renderPassCreateInfo.attachmentCount = attachmentCount;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 0;
	renderPassCreateInfo.pDependencies = nullptr;

	if (vkCreateRenderPass(GetDevice()->GetVkDevice(), &renderPassCreateInfo, nullptr, &m_renderPassHandle) == VK_SUCCESS)
		return true;

	return false;
}

bool CDeviceRenderPass::InitVkFrameBuffer(const CDeviceRenderPassDesc& passDesc)
{
	m_renderTargetCount = 0;
	m_pDepthStencilTarget = nullptr;

	int renderTargetCount;
	std::array<CImageView*, CDeviceRenderPassDesc::MaxRendertargetCount> rendertargetViews;
	auto& renderTargets = passDesc.GetRenderTargets();
	
	CImageView* pDepthStencilView = nullptr;
	auto& depthTarget = passDesc.GetDepthTarget();

	if (!passDesc.GetDeviceRendertargetViews(rendertargetViews, renderTargetCount))
		return false;

	if (!passDesc.GetDeviceDepthstencilView(pDepthStencilView))
		return false;

	VkImageView imageViews[CDeviceRenderPassDesc::MaxRendertargetCount + 1];
	int imageViewCount = 0;

	for (int i = 0; i < renderTargetCount; ++i)
	{
		imageViews[imageViewCount++]           = rendertargetViews[i]->GetHandle();
		m_renderTargets[m_renderTargetCount++] = renderTargets[i].pTexture->GetDevTexture()->Get2DTexture();
	}

	if (pDepthStencilView)
	{
		imageViews[imageViewCount++] = pDepthStencilView->GetHandle();
		m_pDepthStencilTarget         = depthTarget.pTexture->GetDevTexture()->Get2DTexture();
	}

	if (imageViewCount > 0)
	{
		CTexture* pTex = renderTargetCount > 0 ? renderTargets[0].pTexture : depthTarget.pTexture;
		m_frameBufferExtent.width = uint32(pTex->GetWidthNonVirtual());
		m_frameBufferExtent.height = uint32(pTex->GetHeightNonVirtual());

		VkFramebufferCreateInfo frameBufferCreateInfo;

		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = nullptr;
		frameBufferCreateInfo.flags = 0;
		frameBufferCreateInfo.renderPass = m_renderPassHandle;
		frameBufferCreateInfo.attachmentCount = imageViewCount;
		frameBufferCreateInfo.pAttachments = imageViews;
		frameBufferCreateInfo.width = m_frameBufferExtent.width;
		frameBufferCreateInfo.height = m_frameBufferExtent.height;
		frameBufferCreateInfo.layers = 1;

		if (vkCreateFramebuffer(GetDevice()->GetVkDevice(), &frameBufferCreateInfo, nullptr, &m_frameBufferHandle) == VK_SUCCESS)
			return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceObjectFactory::CDeviceObjectFactory()
	: m_fence_handle(0)
	, m_inlineConstantBufferLayout(VK_NULL_HANDLE)
{
	memset(m_NullResources, 0, sizeof(m_NullResources));

	m_frameFenceCounter = 0;
	m_completedFrameFenceCounter = 0;
	for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_frameFences); i++)
		m_frameFences[i] = NULL;

	m_pCoreCommandList.reset(new CDeviceCommandList());
	m_pCoreCommandList->m_sharedState.pCommandList = nullptr;

	m_pVKDevice    = nullptr;
	m_pVKScheduler = nullptr;
}

void CDeviceObjectFactory::AssignDevice(D3DDevice* pDevice)
{
	m_pVKDevice    = pDevice;
	m_pVKScheduler = &m_pVKDevice->GetScheduler();

	m_pVKScheduler->AddQueueEventCallback(m_pCoreCommandList.get(), lambdaCeaseCallback, NCryVulkan::CCommandScheduler::CMDQUEUE_EVENT_CEASE);
	m_pVKScheduler->AddQueueEventCallback(m_pCoreCommandList.get(), lambdaResumeCallback, NCryVulkan::CCommandScheduler::CMDQUEUE_EVENT_RESUME);

	m_pCoreCommandList->m_sharedState.pCommandList = m_pVKScheduler->GetCommandList(CMDQUEUE_GRAPHICS);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSOImpl(const CDeviceGraphicsPSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceGraphicsPSO_Vulkan>(GetDevice());
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSOImpl(const CDeviceComputePSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceComputePSO_Vulkan>(GetDevice());
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceResourceSetPtr CDeviceObjectFactory::CreateResourceSet(CDeviceResourceSet::EFlags flags) const
{
	return std::make_shared<CDeviceResourceSet_Vulkan>(GetDevice(), flags);
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const
{
	auto pResult = std::make_shared<CDeviceResourceLayout_Vulkan>(GetDevice(), resourceLayoutDesc.GetRequiredResourceBindings());
	if (pResult->Init(resourceLayoutDesc))
		return pResult;

	return nullptr;
}

const std::vector<uint8>* CDeviceObjectFactory::LookupResourceLayoutEncoding(uint64 layoutHash)
{
	for (auto& it : m_ResourceLayoutCache)
	{
		auto pLayout = reinterpret_cast<CDeviceResourceLayout_Vulkan*>(it.second.get());
		if (pLayout->GetHash() == layoutHash)
			return &pLayout->GetEncodedLayout();
	}

	return nullptr;
}

VkDescriptorSetLayout CDeviceObjectFactory::GetInlineConstantBufferLayout()
{
	// NOTE: In order to avoid building a set of descriptors for each constant buffer
	// (due to sub allocation) we currently set stageFlags to all possible stages.
	if (m_inlineConstantBufferLayout == VK_NULL_HANDLE)
	{
		VkDescriptorSetLayoutBinding cbBinding;
		cbBinding.binding = 0;
		cbBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		cbBinding.descriptorCount = 1;
		cbBinding.stageFlags = GetShaderStageFlags(EShaderStage_All);
		cbBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.pNext = nullptr;
		layoutCreateInfo.flags = 0;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.pBindings = &cbBinding;

		VkResult result = vkCreateDescriptorSetLayout(GetDevice()->GetVkDevice(), &layoutCreateInfo, nullptr, &m_inlineConstantBufferLayout);
		CRY_ASSERT(result == VK_SUCCESS);
	}

	return m_inlineConstantBufferLayout;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Acquire one or more command-lists which are independent of the core command-list
// Only one thread is allowed to call functions on this command-list (DX12 restriction).
// The thread that gets the permission is the one calling Begin() on it AFAIS
CDeviceCommandListUPtr CDeviceObjectFactory::AcquireCommandList(EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// In theory this whole function need to be atomic, instead it voids the core command-list(s),
	// Synchronization between different threads acquiring command-lists is deferred to the higher level.
	NCryVulkan::CCommandListPool& pQueue = m_pVKScheduler->GetCommandListPool(eQueueType);
	m_pVKScheduler->CeaseAllCommandQueues(false);

	VK_PTR(CCommandList) pCL;
	pQueue.AcquireCommandList(pCL);

	m_pVKScheduler->ResumeAllCommandQueues();
	auto pResult = CryMakeUnique<CDeviceCommandList>();
	pResult->m_sharedState.pCommandList = pCL;
	return pResult;
}

std::vector<CDeviceCommandListUPtr> CDeviceObjectFactory::AcquireCommandLists(uint32 listCount, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// In theory this whole function need to be atomic, instead it voids the core command-list(s),
	// Synchronization between different threads acquiring command-lists is deferred to the higher level.
	NCryVulkan::CCommandListPool& pQueue = m_pVKScheduler->GetCommandListPool(eQueueType);
	m_pVKScheduler->CeaseAllCommandQueues(false);

	std::vector<CDeviceCommandListUPtr> pCommandLists;
	VK_PTR(CCommandList) pCLs[256];

	// Allocate in chunks of 256
	for (uint32 n = 0; n < listCount; n += 256U)
	{
		const uint32 chunkCount = std::min(listCount - n, 256U);
		pQueue.AcquireCommandLists(chunkCount, pCLs);

		for (uint32 b = 0; b < chunkCount; ++b)
		{
			pCommandLists.emplace_back(CryMakeUnique<CDeviceCommandList>());
			pCommandLists.back()->m_sharedState.pCommandList = pCLs[b];
		}
	}

	m_pVKScheduler->ResumeAllCommandQueues();
	return pCommandLists;
}

// Command-list sinks, will automatically submit command-lists in allocation-order
void CDeviceObjectFactory::ForfeitCommandList(CDeviceCommandListUPtr pCommandList, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	NCryVulkan::CCommandListPool& pQueue = m_pVKScheduler->GetCommandListPool(eQueueType);

	if (pCommandList)
	{
		pQueue.ForfeitCommandList(pCommandList->m_sharedState.pCommandList);
	}
}

void CDeviceObjectFactory::ForfeitCommandLists(std::vector<CDeviceCommandListUPtr> pCommandLists, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	NCryVulkan::CCommandListPool& pQueue = m_pVKScheduler->GetCommandListPool(eQueueType);

	const uint32 listCount = pCommandLists.size();
	VK_PTR(CCommandList) pCLs[256];

	// Deallocate in chunks of 256
	for (uint32 n = 0; n < listCount; n += 256U)
	{
		const uint32 chunkCount = std::min(listCount - n, 256U);
		uint32 validCount = 0;

		for (uint32 b = 0; b < chunkCount; ++b)
		{
			if (pCommandLists[b])
			{
				pCLs[validCount++] = pCommandLists[b]->m_sharedState.pCommandList;
			}
		}

		if (validCount)
		{
			pQueue.ForfeitCommandLists(validCount, pCLs);
		}
	}
}

void CDeviceObjectFactory::ReleaseResourcesImpl()
{
	m_pVKDevice->FlushAndWaitForGPU();

	if (m_inlineConstantBufferLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(GetDevice()->GetVkDevice(), m_inlineConstantBufferLayout, nullptr);
		m_inlineConstantBufferLayout = VK_NULL_HANDLE;
	}
}

////////////////////////////////////////////////////////////////////////////
UINT64 CDeviceObjectFactory::QueryFormatSupport(D3DFormat Format)
{
	VkFormat vkFormat = DXGIFormatToVKFormat(Format);
	if (vkFormat != VK_FORMAT_UNDEFINED)
	{
		const SPhysicalDeviceInfo* pInfo = GetVKDevice()->GetPhysicalDeviceInfo();
		const VkFormatProperties& Props = pInfo->formatProperties[vkFormat];
		VkFormatFeatureFlags PropsAny = Props.linearTilingFeatures | Props.optimalTilingFeatures | Props.bufferFeatures;
		VkFormatFeatureFlags PropsTex = Props.linearTilingFeatures | Props.optimalTilingFeatures;

		// *INDENT-OFF*
		return
			(Props.bufferFeatures                                        ? FMTSUPPORT_BUFFER                      : 0) |
			(PropsTex                                                    ? FMTSUPPORT_TEXTURE1D                   : 0) |
			(PropsTex                                                    ? FMTSUPPORT_TEXTURE2D                   : 0) |
			(PropsTex                                                    ? FMTSUPPORT_TEXTURE3D                   : 0) |
			(PropsTex                                                    ? FMTSUPPORT_TEXTURECUBE                 : 0) |
			(Props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT  ? FMTSUPPORT_IA_VERTEX_BUFFER            : 0) |
			(Props.bufferFeatures                                        ? FMTSUPPORT_IA_INDEX_BUFFER             : 0) |
			(false                                                       ? FMTSUPPORT_SO_BUFFER                   : 0) |
			(PropsTex                                                    ? FMTSUPPORT_MIP                         : 0) |
//			(PropsTex                                                    ? FMTSUPPORT_SRGB                        : 0) |
			(true                                                        ? FMTSUPPORT_SHADER_LOAD                 : 0) |
			(true                                                        ? FMTSUPPORT_SHADER_GATHER               : 0) |
			(true                                                        ? FMTSUPPORT_SHADER_GATHER_COMPARISON    : 0) |
			(PropsTex & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT              ? FMTSUPPORT_SHADER_SAMPLE               : 0) |
			(PropsTex & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT              ? FMTSUPPORT_SHADER_SAMPLE_COMPARISON    : 0) |
//			(nOptions & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW ? FMTSUPPORT_TYPED_UNORDERED_ACCESS_VIEW : 0) |
			(PropsTex & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT   ? FMTSUPPORT_DEPTH_STENCIL               : 0) |
			(PropsTex & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT           ? FMTSUPPORT_RENDER_TARGET               : 0) |
			(PropsTex & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT     ? FMTSUPPORT_BLENDABLE                   : 0) |
//			(?                                                           ? FMTSUPPORT_DISPLAYABLE                 : 0) |
			(false /* extension?*/                                       ? FMTSUPPORT_MULTISAMPLE_LOAD            : 0) |
			(false /* extension?*/                                       ? FMTSUPPORT_MULTISAMPLE_RESOLVE         : 0) |
			(false /* extension?*/                                       ? FMTSUPPORT_MULTISAMPLE_RENDERTARGET    : 0);
		// *INDENT-ON*
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Fence API

HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	query = reinterpret_cast<DeviceFenceHandle>(new UINT64);
	hr = query ? S_OK : E_FAIL;
	if (!FAILED(hr))
	{
		IssueFence(query);
	}
	return hr;
}

HRESULT CDeviceObjectFactory::ReleaseFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : S_FALSE;
	delete reinterpret_cast<UINT64*>(query);
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : E_FAIL;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		*handle = m_pVKScheduler->InsertFence();
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = query ? S_FALSE : E_FAIL;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		hr = m_pVKScheduler->TestForFence(*handle);
		if (hr != S_OK)
		{
			// Can only flush from render thread
			CRY_ASSERT(!flush || !gRenDev->m_pRT || gRenDev->m_pRT->IsRenderThread());

			// Test + Flush + No-Block is okay
			// Test + No-Flush + Block may not be okay, caution advised as it could deadlock
			if (flush)
			{
				m_pVKScheduler->FlushToFence(*handle);
			}

			if (block)
			{
				hr = m_pVKScheduler->WaitForFence(*handle);
			}
		}
	}
	return hr;
}

////////////////////////////////////////////////////////////////////////////
// SamplerState API

CDeviceSamplerState* CDeviceObjectFactory::CreateSamplerState(const SSamplerState& state)
{
	CDeviceSamplerState* const pResult = new CDeviceSamplerState(GetDevice());
	if (pResult->Init(state) == VK_SUCCESS)
	{
		return pResult;
	}
	pResult->Release();
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////
// InputLayout API

CDeviceInputLayout* CDeviceObjectFactory::CreateInputLayout(const SInputLayout& pLayout)
{
	VK_NOT_IMPLEMENTED;
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////
// Low-level resource management API (TODO: remove D3D-dependency by abstraction)

void CDeviceObjectFactory::AllocateNullResources()
{
	bool bAnyFailed = false;
	if (!m_NullResources[D3D11_RESOURCE_DIMENSION_BUFFER])
	{
		CDynamicOffsetBufferResource* pNullBuffer;
		VkBufferCreateInfo info;

		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.size = 16;
		info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT       | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
		             VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;

		if (GetDevice()->CreateCommittedResource(kHeapSources, info, &pNullBuffer) == VK_SUCCESS)
		{
			pNullBuffer->CMemoryResource::AddFlags(NCryVulkan::kResourceFlagNull);
			pNullBuffer->SetStrideAndElementCount(16, 1);
			m_NullResources[D3D11_RESOURCE_DIMENSION_BUFFER] = pNullBuffer;

			vkCmdFillBuffer(GetCoreCommandList().GetVKCommandList()->GetVkCommandList(), pNullBuffer->AsBuffer()->GetHandle(), 0, info.size, 0x0);
		}
		else
		{
			bAnyFailed = true;
		}

		m_NullBufferViewStructured = new NCryVulkan::CBufferView(pNullBuffer, 0, 16, VK_FORMAT_UNDEFINED);
		m_NullBufferViewTyped = new NCryVulkan::CBufferView(pNullBuffer, 0, 16, VK_FORMAT_R32G32B32A32_SFLOAT);
	}
	else
	{
		m_NullResources[D3D11_RESOURCE_DIMENSION_BUFFER]->AddRef();
		m_NullBufferViewStructured->AddRef();
		m_NullBufferViewTyped->AddRef();

	}

	for (int dimension = D3D11_RESOURCE_DIMENSION_TEXTURE1D; dimension <= D3D11_RESOURCE_DIMENSION_TEXTURE3D; ++dimension)
	{
		if (!m_NullResources[dimension])
		{
			const VkImageType dimensionality = 
				dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D ? VK_IMAGE_TYPE_1D :
				dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;

			CImageResource* pNullImage;
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
			info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.queueFamilyIndexCount = 0;
			info.pQueueFamilyIndices = nullptr;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (GetDevice()->CreateCommittedResource(kHeapSources, info, &pNullImage) == VK_SUCCESS)
			{
				pNullImage->CMemoryResource::AddFlags(NCryVulkan::kResourceFlagNull);
				m_NullResources[dimension] = pNullImage;

				VkClearColorValue clearColor = {{ 0 }};
				VkImageSubresourceRange clearRange;
				clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				clearRange.baseArrayLayer = 0;
				clearRange.baseMipLevel = 0;
				clearRange.layerCount = info.arrayLayers;
				clearRange.levelCount = info.mipLevels;

				pNullImage->TrackAnnouncedAccess(VK_ACCESS_TRANSFER_WRITE_BIT);
				pNullImage->TransitionBarrier(GetCoreCommandList().GetVKCommandList(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

				// TODO: add to API
				GetCoreCommandList().GetVKCommandList()->PendingResourceBarriers();
				vkCmdClearColorImage(GetCoreCommandList().GetVKCommandList()->GetVkCommandList(), pNullImage->GetHandle(), pNullImage->GetLayout(), &clearColor, 1, &clearRange);
			}
			else
			{
				bAnyFailed = true;
			}
		}
		else
		{
			m_NullResources[dimension]->AddRef();
		}
	}
	VK_ASSERT(!bAnyFailed && "Cannot create null resources, this will cause issues for resource-sets using them!");
}

void CDeviceObjectFactory::ReleaseNullResources()
{
	SAFE_RELEASE(m_NullResources[D3D11_RESOURCE_DIMENSION_BUFFER]);
	SAFE_RELEASE(m_NullResources[D3D11_RESOURCE_DIMENSION_TEXTURE1D]);
	SAFE_RELEASE(m_NullResources[D3D11_RESOURCE_DIMENSION_TEXTURE2D]);
	SAFE_RELEASE(m_NullResources[D3D11_RESOURCE_DIMENSION_TEXTURE3D]);
}

void CDeviceObjectFactory::UpdateDeferredUploads()
{
	AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_deferredUploadCS);

	for (auto& upload : m_deferredUploads)
	{
		if (CImageResource* const pImage = upload.pTarget->AsImage())
		{
			GetCoreCommandList().GetCopyInterface()->UploadImage(
				upload.pStagingBuffer,
				pImage,
				upload.targetMapping,
				upload.bExtendedAdressing);
		}
		else if (CBufferResource* const pBuffer = upload.pTarget->AsBuffer())
		{
			SResourceRegionMapping mapping;
			mapping.DestinationOffset = upload.targetMapping.ResourceOffset;
			mapping.Extent = upload.targetMapping.Extent;
			mapping.Flags = upload.targetMapping.Flags;
			mapping.SourceOffset.Left = 0;
			mapping.SourceOffset.Front = 0;
			mapping.SourceOffset.Top = 0;
			mapping.SourceOffset.Subresource = 0;

			GetCoreCommandList().GetCopyInterface()->CopyBuffer(
				upload.pStagingBuffer,
				pBuffer,
				mapping);
		}
	}

	m_deferredUploads.clear();
}

//=============================================================================

#ifdef DEVRES_USE_STAGING_POOL
D3DResource* CDeviceObjectFactory::AllocateStagingResource(D3DResource* pForTex, bool bUpload, void*& pMappedAddress)
{
	VkResult result = VK_ERROR_FEATURE_NOT_PRESENT;
	CBufferResource* pStaging = nullptr;

	if (CBufferResource* pBuffer = pForTex->AsBuffer())
	{
		const VkDeviceSize minSize = pBuffer->GetStride() * pBuffer->GetElementCount();
		result = GetDevice()->CreateOrReuseStagingResource(pBuffer, minSize, &pStaging, bUpload);
	}
	else if (CImageResource* pImage = pForTex->AsImage())
	{
		SResourceMemoryMapping mapping;
		SelectStagingLayout(pImage, 0, mapping);
		result = GetDevice()->CreateOrReuseStagingResource(pImage, mapping.MemoryLayout.volumeStride, &pStaging, bUpload);
	}

	if (result == VK_SUCCESS)
	{
		if (pMappedAddress = pStaging->Map())
		{
			return pStaging;
		}

		pStaging->Release(); // trigger ReleaseLater with kResourceFlagReusable
	}

	VK_ASSERT(false && "Upload buffer skipped: Cannot create staging buffer");
	return nullptr;
}

void CDeviceObjectFactory::ReleaseStagingResource(D3DResource* pStaging)
{
	pStaging->Unmap();
	pStaging->Release(); // trigger ReleaseLater with kResourceFlagReusable
}
#endif

//=============================================================================
// TODO: Function should be in DeviceObjects_Vulkan.inl
template<typename T>
static inline EHeapType SuggestVKHeapType(const T& desc)
{
	CRY_ASSERT_MESSAGE((desc & (CDeviceObjectFactory::USAGE_CPU_READ | CDeviceObjectFactory::USAGE_CPU_WRITE)) != (CDeviceObjectFactory::USAGE_CPU_READ | CDeviceObjectFactory::USAGE_CPU_WRITE), "CPU Read and Write can't be requested together!");

	// *INDENT-OFF*
	return EHeapType(
		((desc                & (CDeviceObjectFactory::USAGE_CPU_READ       ) ? kHeapDownload :
		((desc                & (CDeviceObjectFactory::USAGE_CPU_WRITE      ) ? kHeapUpload :
		((desc                & (CDeviceObjectFactory::BIND_DEPTH_STENCIL   |
		                         CDeviceObjectFactory::BIND_RENDER_TARGET   |
		                         CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? kHeapTargets :
		                                                                        kHeapSources)))))));
	// *INDENT-ON*
}

// Set info.usage and info.tiling based on usage flag, returns an appropriate heap.
static EHeapType ConfigureUsage(VkImageCreateInfo& info, uint32 nUsage)
{
	info.usage = ConvertToVKImageUsageBits(nUsage);

	info.tiling = ConvertToVKImageTiling(nUsage);
	if (info.tiling == VK_IMAGE_TILING_LINEAR)
	{
		// CPU access of some kind requested, this will fail on anything except copy-only resources!
		VK_ASSERT((info.usage & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) == 0 && "CPU access not allowed on images, when the image is not copy-only");
	}

	return SuggestVKHeapType(nUsage);
}

// Set info.usage, returns an appropriate heap.
static EHeapType ConfigureUsage(VkBufferCreateInfo& info, uint32 nUsage)
{
	info.usage = ConvertToVKBufferUsageBits(nUsage);

	return SuggestVKHeapType(nUsage);
}

void CDeviceObjectFactory::UploadInitialImageData(SSubresourcePayload pSrcMips[], CImageResource* pDst, const VkImageCreateInfo& info)
{
	CRY_ASSERT(!(info.arrayLayers > 1 && info.extent.depth > 1)); // arrays of volumes not supported

	const bool bImmediateUpload = gcpRendD3D->m_pRT->IsRenderThread() && !gcpRendD3D->m_pRT->IsRenderLoadingThread();

	uint32_t width  = info.extent.width;
	uint32_t height = info.extent.height;
	uint32_t depth  = info.extent.depth;

	uint32_t blockWidth = 1, blockHeight = 1, blockBytes;
	const bool bIsBlocks = GetBlockSize(info.format, blockWidth, blockHeight, blockBytes);
	if (!bIsBlocks)
	{
		blockBytes = pSrcMips[0].m_sSysMemAlignment.rowStride / width;
	}

	SResourceMemoryMapping imageMapping;
	SResourceMemoryMapping bufferMapping;

	imageMapping.MemoryLayout.typeStride = blockBytes;
	imageMapping.ResourceOffset.Left = 0;
	imageMapping.ResourceOffset.Top = 0;
	imageMapping.ResourceOffset.Front = 0;
	imageMapping.Extent.Subresources = (info.arrayLayers - 1) * info.mipLevels + 1; // Select all the slices in this mip (non-standard addressing!)

	bufferMapping.ResourceOffset.Subresource = 0;
	bufferMapping.Extent.Subresources = 1;

	for (uint32_t mip = 0; mip < info.mipLevels; ++mip)
	{
		const SSubresourcePayload* const pData = pSrcMips + mip;

		// Image mapping for all slices.
		imageMapping.MemoryLayout.rowStride    = pData->m_sSysMemAlignment.rowStride;
		imageMapping.MemoryLayout.planeStride  = pData->m_sSysMemAlignment.planeStride;
		imageMapping.MemoryLayout.volumeStride = pData->m_sSysMemAlignment.planeStride * info.arrayLayers * depth;
		imageMapping.ResourceOffset.Subresource = mip; // Select given mip on first array slice
		imageMapping.Extent.Width  = width;
		imageMapping.Extent.Height = height;
		imageMapping.Extent.Depth  = depth;

		// Buffer mapping for one slice.
		bufferMapping.ResourceOffset.Left = 0;
		bufferMapping.MemoryLayout = imageMapping.MemoryLayout;
		bufferMapping.Extent.Width = bufferMapping.MemoryLayout.volumeStride = pData->m_sSysMemAlignment.planeStride * depth;

		// Create buffer for all slices.
		CBufferResource* pStagingRaw = nullptr;
		if (GetDevice()->CreateOrReuseStagingResource(pDst, imageMapping.MemoryLayout.volumeStride, &pStagingRaw, true) != VK_SUCCESS) 
		{
			VK_ASSERT(false && "Upload buffer skipped: Cannot create staging buffer");
			return;
		}
		_smart_ptr<CBufferResource> pStaging;
		pStaging.Assign_NoAddRef(pStagingRaw);

		// Fill first slice of the array.
		bool bFilled = CDeviceCopyCommandInterface::FillBuffer(pData->m_pSysMem, pStaging, bufferMapping);

		// Fill additional slices of the array (if needed).
		// The slices must have identical layout as the first slice.
		const SSubresourcePayload* pSlice = pData;
		for (uint32_t slice = 1; slice < info.arrayLayers; ++slice)
		{
			pSlice += info.mipLevels;
			bufferMapping.ResourceOffset.Left += pData->m_sSysMemAlignment.planeStride;
			
			VK_ASSERT(pSlice->m_sSysMemAlignment.rowStride == pData->m_sSysMemAlignment.rowStride && pSlice->m_sSysMemAlignment.planeStride == pData->m_sSysMemAlignment.planeStride && "Slice data not compatible");
			bFilled &= CDeviceCopyCommandInterface::FillBuffer(pSlice->m_pSysMem, pStaging, bufferMapping);
		}

		if (bFilled)
		{
			const bool bExtendedAddressing = true;

			if (bImmediateUpload)
			{
				GetCoreCommandList().GetCopyInterface()->UploadImage(pStaging, pDst, imageMapping, bExtendedAddressing);
			}
			else
			{
				SDeferredUploadData uploadData;
				uploadData.pStagingBuffer = std::move(pStaging);
				uploadData.pTarget = pDst;
				uploadData.targetMapping = imageMapping;
				uploadData.bExtendedAdressing = bExtendedAddressing;

				AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_deferredUploadCS);
				m_deferredUploads.push_back(uploadData);
			}
		}

		width  = std::max(width  >> 1, blockWidth);
		height = std::max(height >> 1, blockHeight);
		depth  = std::max(depth  >> 1, 1U);
	}
}

void CDeviceObjectFactory::SelectStagingLayout(const NCryVulkan::CImageResource* pImage, uint32 subResource, SResourceMemoryMapping& result)
{
	const uint32 mip = subResource % pImage->GetMipCount();
	const uint32 slice = subResource / pImage->GetMipCount();
	VK_ASSERT(mip < pImage->GetMipCount() && slice < pImage->GetSliceCount() && "Bad subresource selected");

	const uint32 width = std::max(pImage->GetWidth() >> mip, 1U);
	const uint32 height = std::max(pImage->GetHeight() >> mip, 1U);
	const uint32 depth = std::max(pImage->GetDepth() >> mip, 1U);

	uint32 blockWidth = 1, blockHeight = 1, blockBytes;
	const bool bIsBlocks = GetBlockSize(pImage->GetFormat(), blockWidth, blockHeight, blockBytes);
	if (!bIsBlocks)
	{
		blockBytes = s_FormatWithSize[s_VkFormatToDXGI[pImage->GetFormat()]].Size; // TODO: Use a better API for this?
		VK_ASSERT(blockBytes >= 1 && blockBytes <= 16 && "Unknown texel size");
	}

	const uint32 widthInBlocks = (width + blockWidth - 1U) / blockWidth;
	const uint32 heightInBlocks = (height + blockHeight - 1U) / blockHeight;

	result.MemoryLayout = SResourceMemoryAlignment::Linear(blockBytes, widthInBlocks, heightInBlocks, depth);
	result.ResourceOffset.Left = 0;
	result.ResourceOffset.Top = 0;
	result.ResourceOffset.Front = 0;
	result.ResourceOffset.Subresource = subResource;
	result.Extent.Width = width;
	result.Extent.Height = height;
	result.Extent.Depth = depth;
	result.Extent.Subresources = 1;
	result.Flags = 0;
}

HRESULT CDeviceObjectFactory::Create2DTexture(uint32 nWidth, uint32 nHeight, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI, int32 nESRAMOffset)
{
	VkImageCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = ConvertFormat(Format);
	info.extent.width = nWidth;
	info.extent.height = nHeight;
	info.extent.depth = 1;
	info.mipLevels = nMips;
	info.arrayLayers = nArraySize;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const EHeapType heapHint = ConfigureUsage(info, nUsage);

	if (pTI)
	{
		info.samples = (VkSampleCountFlagBits)pTI->m_nDstMSAASamples;
	}

	if (info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		// R16_TYPELESS/_UNORM and R32_TYPELESS/_FLOAT in Vk are NOT aliases of D16_UNORM/D32_FLOAT
		// In this case, ConvertFormat doesn't do the right thing since it doesn't check usage flags.
		switch (Format)
		{
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_TYPELESS:
			info.format = VK_FORMAT_D16_UNORM;
			break;
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_TYPELESS:
			info.format = VK_FORMAT_D32_SFLOAT;
			break;
		}

		// Anything depth/stencil can NEVER be interpreted as some other format (Vk limitation).
		// Therefore, remove the VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT flag.
		info.flags &= ~VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

	VK_ASSERT(nWidth * nHeight * nArraySize * nMips != 0);
	CImageResource* pResult = nullptr;
	VkResult hResult = VK_SUCCESS;
	if (nUsage & USAGE_HIFREQ_HEAP)
		hResult = GetDevice()->CreateOrReuseCommittedResource(heapHint, info, &pResult);
	else
		hResult = GetDevice()->CreateCommittedResource(heapHint, info, &pResult);

	if (hResult != VK_SUCCESS)
	{
		return E_FAIL;
	}

	CDeviceTexture* const pWrapper = (*ppDevTexture = new CDeviceTexture());
	if (pTI && pTI->m_pSysMemSubresourceData)
	{
		VK_ASSERT(pTI->m_eSysMemTileMode == eTM_None && "Tiled upload not supported");
		UploadInitialImageData(pTI->m_pSysMemSubresourceData, pResult, info);
	}

	pWrapper->m_pNativeResource = pResult;
	pWrapper->m_nBaseAllocatedSize = (size_t)pResult->GetSize();

	return S_OK;
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(uint32 nSize, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
	VkImageCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = ConvertFormat(Format);
	info.extent.width = nSize;
	info.extent.height = nSize;
	info.extent.depth = 1;
	info.mipLevels = nMips;
	info.arrayLayers = nArraySize; assert(!(nArraySize % 6));
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const EHeapType heapHint = ConfigureUsage(info, nUsage);

	if (pTI)
	{
		info.samples = (VkSampleCountFlagBits)pTI->m_nDstMSAASamples;
	}

	VK_ASSERT(nSize * nSize * nArraySize * nMips != 0);
	CImageResource* pResult = nullptr;
	VkResult hResult = VK_SUCCESS;
	if (nUsage & USAGE_HIFREQ_HEAP)
		hResult = GetDevice()->CreateOrReuseCommittedResource(heapHint, info, &pResult);
	else
		hResult = GetDevice()->CreateCommittedResource(heapHint, info, &pResult);

	if (hResult != VK_SUCCESS)
	{
		return E_FAIL;
	}

	CDeviceTexture* const pWrapper = (*ppDevTexture = new CDeviceTexture());
	if (pTI && pTI->m_pSysMemSubresourceData)
	{
		VK_ASSERT(pTI->m_eSysMemTileMode == eTM_None && "Tiled upload not supported");
		UploadInitialImageData(pTI->m_pSysMemSubresourceData, pResult, info);
	}

	pWrapper->m_pNativeResource = pResult;
	pWrapper->m_nBaseAllocatedSize = (size_t)pResult->GetSize();

	return S_OK;
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
	VkImageCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	info.imageType = VK_IMAGE_TYPE_3D;
	info.format = ConvertFormat(Format);
	info.extent.width = nWidth;
	info.extent.height = nHeight;
	info.extent.depth = nDepth;
	info.mipLevels = nMips;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const EHeapType heapHint = ConfigureUsage(info, nUsage);

	if (pTI)
	{
		VK_ASSERT(pTI->m_nDstMSAASamples == 1 && "MSAA samples not supported on 3D textures");
	}

	VK_ASSERT(nWidth * nHeight * nDepth * nMips != 0);
	CImageResource* pResult = nullptr;
	VkResult hResult = VK_SUCCESS;
	if (nUsage & USAGE_HIFREQ_HEAP)
		hResult = GetDevice()->CreateOrReuseCommittedResource(heapHint, info, &pResult);
	else
		hResult = GetDevice()->CreateCommittedResource(heapHint, info, &pResult);

	if (hResult != VK_SUCCESS)
	{
		return E_FAIL;
	}

	CDeviceTexture* const pWrapper = (*ppDevTexture = new CDeviceTexture());
	if (pTI && pTI->m_pSysMemSubresourceData)
	{
		VK_ASSERT(pTI->m_eSysMemTileMode == eTM_None && "Tiled upload not supported");
		UploadInitialImageData(pTI->m_pSysMemSubresourceData, pResult, info);
	}

	pWrapper->m_pNativeResource = pResult;
	pWrapper->m_nBaseAllocatedSize = (size_t)pResult->GetSize();

	return S_OK;
}

HRESULT CDeviceObjectFactory::CreateBuffer(buffer_size_t nSize, buffer_size_t elemSize, uint32 nUsage, uint32 nBindFlags, D3DBuffer** ppBuff, const void* pData)
{
	VkBufferCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.size = nSize * elemSize;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	const EHeapType heapHint = ConfigureUsage(info, nUsage | nBindFlags);

	VK_ASSERT(nSize * elemSize != 0);
	CBufferResource* pResult = nullptr;
	VkResult hResult = VK_SUCCESS;
	if (nUsage & USAGE_HIFREQ_HEAP)
	{
		hResult = GetDevice()->CreateOrReuseCommittedResource(heapHint, info, &pResult);
	}
	else if (nBindFlags & BIND_CONSTANT_BUFFER)
	{
		CDynamicOffsetBufferResource* pDynamicBuffer = nullptr;
		GetDevice()->CreateCommittedResource(heapHint, info, &pDynamicBuffer);
		pResult = pDynamicBuffer;
	}
	else
	{
		hResult = GetDevice()->CreateCommittedResource(heapHint, info, &pResult);
	}

	if (hResult != VK_SUCCESS)
	{
		return E_FAIL;
	}

	pResult->SetStrideAndElementCount(elemSize, nSize);

	*ppBuff = pResult;
	if (pData)
	{
		SResourceMemoryMapping mapping;
		ZeroStruct(mapping);

		mapping.ResourceOffset.Left = 0 * elemSize;
		mapping.ResourceOffset.Subresource = 0;
		mapping.Extent.Width = nSize * elemSize;
		mapping.Extent.Subresources = 1;
		mapping.MemoryLayout = SResourceMemoryAlignment::Linear(elemSize, nSize);

		const bool bImmediateUpload = gcpRendD3D->m_pRT->IsRenderThread() && !gcpRendD3D->m_pRT->IsRenderLoadingThread();

		CDeviceCommandListRef commandList = GetCoreCommandList();
		if (CBufferResource* const pStaging = commandList.GetCopyInterface()->UploadBuffer(pData, pResult, mapping, bImmediateUpload))
		{
			// Unable to perform upload immediately (not called from render-thread and the desired buffer is not CPU writable).
			// We have to perform the staging at the earliest convenience of the render-thread instead.
			SDeferredUploadData uploadData;
			uploadData.pStagingBuffer.Assign_NoAddRef(pStaging);
			uploadData.pTarget = pResult;
			uploadData.targetMapping = mapping;
			uploadData.bExtendedAdressing = false;

			AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_deferredUploadCS);
			m_deferredUploads.push_back(uploadData);
		}
	}

	return S_OK;
}

//=============================================================================

#if !(CRY_PLATFORM_DURANGO && BUFFER_ENABLE_DIRECT_ACCESS)
HRESULT CDeviceObjectFactory::InvalidateCpuCache(void* buffer_ptr, size_t size, size_t offset)
{
	return S_OK;
}

HRESULT CDeviceObjectFactory::InvalidateGpuCache(D3DBuffer* buffer, void* buffer_ptr, buffer_size_t size, buffer_size_t offset)
{
	return S_OK;
}
#endif

void CDeviceObjectFactory::InvalidateBuffer(D3DBuffer* buffer, void* base_ptr, buffer_size_t offset, buffer_size_t size, uint32 id)
{
}

//=============================================================================

uint8* CDeviceObjectFactory::Map(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	VK_ASSERT(mode == D3D11_MAP_WRITE_NO_OVERWRITE || mode == D3D11_MAP_READ); // NOTE: access to resources in use by the gpu MUST be synchronized externally
	return reinterpret_cast<uint8*>(buffer->Map());
}

void CDeviceObjectFactory::Unmap(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	VK_ASSERT(mode == D3D11_MAP_WRITE_NO_OVERWRITE || mode == D3D11_MAP_READ); // NOTE: access to resources in use by the gpu MUST be synchronized externally
	buffer->Unmap();
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::UploadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks)
{
	VK_ASSERT(mode == D3D11_MAP_WRITE_NO_OVERWRITE); // only no-overwrite supported currently

	if (!bDirectAccess)
	{
		const uint8* pInData = reinterpret_cast<const uint8*>(pInDataCPU);
		uint8* pOutData = CDeviceObjectFactory::Map(buffer, subresource, offset, 0, mode) + offset;

		StreamBufferData(pOutData, pInData, size);

		CDeviceObjectFactory::Unmap(buffer, subresource, offset, size, mode);
	}
	else
	{
		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::MarkReadRange(buffer, offset, 0, D3D11_MAP_WRITE);

		StreamBufferData(pOutDataGPU, pInDataCPU, size);

		CDeviceObjectFactory::MarkWriteRange(buffer, offset, size, D3D11_MAP_WRITE);
	}
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::DownloadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks)
{
	VK_ASSERT(buffer->GetHeapType() == kHeapDownload);
	VK_ASSERT(mode == D3D11_MAP_READ); // NOTE: access to resources in use by the gpu MUST be synchronized externally
	VK_ASSERT(pInDataGPU == nullptr);  // Currently not handled

	uint8* pInData = CDeviceObjectFactory::Map(buffer, subresource, offset, size, mode) + offset;
	memcpy(pOutDataCPU, pInData, size);
	CDeviceObjectFactory::Unmap(buffer, subresource, offset, size, mode);
}

// Explicit instantiation
template
void CDeviceObjectFactory::UploadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::UploadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);

template
void CDeviceObjectFactory::DownloadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::DownloadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);

// Shader API
static NCryVulkan::CShader* CreateShader(NCryVulkan::CDevice* pDevice, const void* pShaderBytecode, size_t bytecodeLength)
{
	auto pShader = new NCryVulkan::CShader(pDevice);
	if (pShader->Init(pShaderBytecode, bytecodeLength) == VK_SUCCESS)
	{
		return pShader;
	}
	pShader->Release();
	return nullptr;
}

ID3D11VertexShader* CDeviceObjectFactory::CreateVertexShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11PixelShader* CDeviceObjectFactory::CreatePixelShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11GeometryShader* CDeviceObjectFactory::CreateGeometryShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11HullShader* CDeviceObjectFactory::CreateHullShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11DomainShader* CDeviceObjectFactory::CreateDomainShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11ComputeShader* CDeviceObjectFactory::CreateComputeShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

// Occlusion Query API
D3DOcclusionQuery* CDeviceObjectFactory::CreateOcclusionQuery()
{
	std::unique_ptr<D3DOcclusionQuery> pResult(new NCryVulkan::COcclusionQuery(GetVKDevice()->GetOcclusionQueries()));
	return pResult->IsValid() ? pResult.release() : nullptr;
}

bool CDeviceObjectFactory::GetOcclusionQueryResults(D3DOcclusionQuery* pQuery, uint64& samplesPassed)
{
	return pQuery->GetResults(samplesPassed);
}
