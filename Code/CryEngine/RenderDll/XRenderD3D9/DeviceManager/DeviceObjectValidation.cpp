// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DeviceObjectValidation.h"

CDeviceObjectValidator CDeviceObjectValidator::Create()
{
	CDeviceObjectValidator validator;

	// *INDENT-OFF*
	// Per draw or dispatch
	validator.Limits.PerDraw.NumBindings              = 4;   // internal PS4 limitation
	validator.Limits.PerDraw.NumSamplers              = 14;  // internal PS4 limitation
	validator.Limits.PerDraw.NumConstantBuffers       = 14;  // internal PS4 limitation
	validator.Limits.PerDraw.NumShaderResources       = 32;  // internal PS4 limitation
	validator.Limits.PerDraw.NumInlineConstantBuffers = 4;   // internal PS4 limitation
	validator.Limits.PerDraw.NumInlineShaderResources = 4;   // internal PS4 limitation
	validator.Limits.PerDraw.NumBuffers               = 32;  // internal dx11 limitation
	validator.Limits.PerDraw.NumBufferSRVs            = 32;  // internal dx11 limitation
	validator.Limits.PerDraw.NumBufferUAVs            = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.NumTextures              = 62;  // internal PS4 limitation
	validator.Limits.PerDraw.NumTextureSRVs           = 54;  // internal PS4 limitation
	validator.Limits.PerDraw.NumTextureUAVs           = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.NumUAVs                  = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.NumSRVs                  = 54;  // internal PS4 limitation

	 // Per shader stage
	validator.Limits.PerDraw.PerShaderStage.NumSamplers        = 14;  // internal PS4 limitation
	validator.Limits.PerDraw.PerShaderStage.NumShaderResources = 91;  // internal PS4 limitation
	validator.Limits.PerDraw.PerShaderStage.NumConstantBuffers = 12;  // Vulkan: NVidia GTX 660
	validator.Limits.PerDraw.PerShaderStage.NumBuffers         = 16;  // Vulkan: NVidia GTX 660
	validator.Limits.PerDraw.PerShaderStage.NumBufferSRVs      = 16;  // Vulkan: NVidia GTX 660
	validator.Limits.PerDraw.PerShaderStage.NumBufferUAVs      = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.PerShaderStage.NumTextures        = 62;  // internal PS4 limitation
	validator.Limits.PerDraw.PerShaderStage.NumTextureSRVs     = 54;  // internal PS4 limitation
	validator.Limits.PerDraw.PerShaderStage.NumTextureUAVs     = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.PerShaderStage.NumUAVs            = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.PerShaderStage.NumSRVs            = 54;  // internal PS4 limitation

	// Per resource set
	validator.Limits.PerResourceSet.NumBuffers                 = 8;   // internal dx11 limitation

	// *INDENT-ON*

	return validator;
}

CDeviceObjectValidator CDeviceObjectValidator::CreateForMobile()
{
	CDeviceObjectValidator validator;

	// *INDENT-OFF*
	/* Per draw or dispatch */
	validator.Limits.PerDraw.NumBindings              = 4;   // Mali t880
	validator.Limits.PerDraw.NumSamplers              = 14;  // internal PS4 limitation
	validator.Limits.PerDraw.NumConstantBuffers       = 24;  // internal PS4 limitation
	validator.Limits.PerDraw.NumConstantBuffers       = 14;  // internal PS4 limitation
	validator.Limits.PerDraw.NumInlineConstantBuffers = 4;   // internal PS4 limitation
	validator.Limits.PerDraw.NumInlineShaderResources = 4;   // internal PS4 limitation
	validator.Limits.PerDraw.NumBuffers               = 24;  // Mali t880
	validator.Limits.PerDraw.NumBufferSRVs            = 24;  // Mali t880
	validator.Limits.PerDraw.NumBufferUAVs            = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.NumTextures              = 62;  // internal PS4 limitation
	validator.Limits.PerDraw.NumTextureSRVs           = 54;  // internal PS4 limitation
	validator.Limits.PerDraw.NumTextureUAVs           = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.NumUAVs                  = 8;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.NumSRVs                  = 54;  // internal PS4 limitation

	/*Per shader stage*/
	validator.Limits.PerDraw.PerShaderStage.NumSamplers              = 14;  // internal PS4 limitation
	validator.Limits.PerDraw.PerShaderStage.NumConstantBuffers       = 12;  // Mali t880
	validator.Limits.PerDraw.PerShaderStage.NumShaderResources       = 44;  // Mali t880
	validator.Limits.PerDraw.PerShaderStage.NumBuffers               = 4;   // Mali t880
	validator.Limits.PerDraw.PerShaderStage.NumBufferSRVs            = 4;   // Mali t880
	validator.Limits.PerDraw.PerShaderStage.NumBufferUAVs            = 4;   // Mali t880
	validator.Limits.PerDraw.PerShaderStage.NumTextures              = 20;  // Mali t880
	validator.Limits.PerDraw.PerShaderStage.NumTextureSRVs           = 16;  // Mali t880
	validator.Limits.PerDraw.PerShaderStage.NumTextureUAVs           = 4;   // dx11: max uav count PS/CS
	validator.Limits.PerDraw.PerShaderStage.NumUAVs                  = 8;   // Mali t880
	validator.Limits.PerDraw.PerShaderStage.NumSRVs                  = 20;  // Mali t880

	// Per resource set
	validator.Limits.PerResourceSet.NumBuffers = 8;   // internal dx11 limitation

	// *INDENT-ON*

	return validator;
}

bool CDeviceObjectValidator::ValidateResourceLayout(const SDeviceResourceLayoutDesc& layoutDesc) const
{
	// VALIDATION RULES:
	// * Cannot bind multiple resources to same layout (CPU side) slot
	// * Cannot have gaps in layout (CPU side) slots
	// * Cannot bind multiple resources to same shader slot
	//   -> Due to dx12, even things like tex0 => (t0, EShaderStage_Vertex), tex1 => (t0, EShaderStage_Pixel) are invalid
	// * Check against device limits stored in Limits struct

	SDeviceLimits::SPerDraw perDrawResources;
	SDeviceLimits::SPerDraw::SPerStageLimits perStageResources[eHWSC_Num];

	ZeroStruct(perDrawResources);
	ZeroArray(perStageResources);

	///////////////////////////////////////////// Stage 1: record all bindings and check for overlaps /////////////////////////////////////////////
#if defined(USE_CRY_ASSERT)
	auto GetResourceName = [](const SResourceBinding& resource)
	{
		switch (resource.type)
		{
		case SResourceBinding::EResourceType::Texture:        return resource.pTexture->GetName();
		case SResourceBinding::EResourceType::Buffer:         return "GpuBuffer";
		case SResourceBinding::EResourceType::ConstantBuffer: return "ConstantBuffer";
		case SResourceBinding::EResourceType::Sampler:        return "Sampler";
		case SResourceBinding::EResourceType::InvalidType:    return "Void";
		};
		return "Unknown";
	};

	auto GetBindPointName = [](SResourceBindPoint bindPoint)
	{
		static char buffer[64];
		char slotPrefix[] = { 'b', 't', 'u', 's' };

		cry_sprintf(buffer, "%c%d", slotPrefix[min((size_t)bindPoint.slotType, CRY_ARRAY_COUNT(slotPrefix) - 1)], bindPoint.slotNumber);

		return buffer;
	};
#endif
	std::set<uint32> usedLayoutBindSlots;
	std::map<int16, SResourceBinding> usedShaderBindSlotsS[(uint8)SResourceBindPoint::ESlotType::Count][eHWSC_Num]; // used slot numbers per slot type and shader stage
	std::map<int16, SResourceBinding> usedShaderBindSlotsL[(uint8)SResourceBindPoint::ESlotType::Count][EResourceLayoutSlot_Max + 1];

	auto validateLayoutSlot = [&](uint32 layoutSlot)
	{
		if (usedLayoutBindSlots.insert(layoutSlot).second == false)
		{
			CRY_ASSERT_TRACE(false, ("Invalid Resource Layout: Multiple resources on layout (CPU side) slot %d", layoutSlot));
			return false;
		}

		return true;
	};

	auto isInline = [](SDeviceResourceLayoutDesc::SLayoutBindPoint bindPointLayout)
	{
		return bindPointLayout.slotType == SDeviceResourceLayoutDesc::ELayoutSlotType::InlineConstantBuffer;
	};

	auto isUAV = [](SResourceBindPoint bindPoint)
	{
		return bindPoint.slotType == SResourceBindPoint::ESlotType::UnorderedAccessView;
	};

	//layoutDesc.m_resourceBindings[]
	auto validateResourceBindPoint = [&](SDeviceResourceLayoutDesc::SLayoutBindPoint bindPointLayout, SResourceBindPoint bindPoint, const SResourceBinding& resource)
	{
		const uint8 layoutSlot = bindPointLayout.layoutSlot;

		// add to per draw resource count
		// *INDENT-OFF*
		switch (resource.type)
		{
		case SResourceBinding::EResourceType::ConstantBuffer:                                   isInline(bindPointLayout) ?  ++perDrawResources.NumInlineConstantBuffers                    :  ++perDrawResources.NumConstantBuffers;                              break;
		case SResourceBinding::EResourceType::ShaderResource:                                   isInline(bindPointLayout) ?  ++perDrawResources.NumInlineShaderResources                     : ++perDrawResources.NumShaderResources;                              break;
		case SResourceBinding::EResourceType::Texture:          ++perDrawResources.NumTextures; isUAV(bindPoint)          ? (++perDrawResources.NumTextureUAVs, ++perDrawResources.NumUAVs) : (++perDrawResources.NumTextureSRVs, ++perDrawResources.NumSRVs);     break;
		case SResourceBinding::EResourceType::Buffer:           ++perDrawResources.NumBuffers;  isUAV(bindPoint)          ? (++perDrawResources.NumBufferUAVs,  ++perDrawResources.NumUAVs) : (++perDrawResources.NumBufferSRVs, ++perDrawResources.NumSRVs);      break;
		case SResourceBinding::EResourceType::Sampler:          ++perDrawResources.NumSamplers; break;
		}
		// *INDENT-ON*

		// Shader stages are ordered by usage-frequency and loop exists according to usage-frequency (VS+PS fast, etc.)
		int validShaderStages = bindPoint.stages;
		for (EHWShaderClass shaderClass = eHWSC_Vertex; validShaderStages; shaderClass = EHWShaderClass(shaderClass + 1), validShaderStages >>= 1)
		{
			if (validShaderStages & 1)
			{
				// Across all layouts, no stage-local slot can be referenced twice
				auto insertResultS = usedShaderBindSlotsS[uint8(bindPoint.slotType)][shaderClass].insert(std::make_pair(bindPoint.slotNumber, resource));
				if (insertResultS.second == false)
				{
#if defined(USE_CRY_ASSERT)
					auto& existingResource = insertResultS.first->second;
					auto& currentResource = resource;

					CRY_ASSERT_TRACE(false, ("Invalid Resource Layout : Multiple resources bound to shader slot %s across multiple layoutSlots: A: %s - B: %s",
						GetBindPointName(bindPoint), GetResourceName(existingResource), GetResourceName(currentResource)));
#endif

					SDeviceLimits::SPerDraw::SPerStageLimits& shadeStageResource = perStageResources[shaderClass];
					// *INDENT-OFF*
					switch (resource.type)
					{
					case SResourceBinding::EResourceType::Texture:          ++shadeStageResource.NumTextures; isUAV(bindPoint) ? (++shadeStageResource.NumTextureUAVs, ++shadeStageResource.NumUAVs) : (++shadeStageResource.NumTextureSRVs, ++shadeStageResource.NumSRVs);     break;
					case SResourceBinding::EResourceType::Buffer:           ++shadeStageResource.NumBuffers;  isUAV(bindPoint) ? (++shadeStageResource.NumBufferUAVs,  ++shadeStageResource.NumUAVs) : (++shadeStageResource.NumBufferSRVs,  ++shadeStageResource.NumSRVs);     break;
					case SResourceBinding::EResourceType::ConstantBuffer:   ++shadeStageResource.NumConstantBuffers; break;
					case SResourceBinding::EResourceType::ShaderResource:   ++shadeStageResource.NumShaderResources; break;
					case SResourceBinding::EResourceType::Sampler:          ++shadeStageResource.NumSamplers;        break;
					}
					// *INDENT-ON*
				}
			}
		}

		// Across all stages, no layout-local slot can be referenced twice
		auto insertResultL = usedShaderBindSlotsL[uint8(bindPoint.slotType)][layoutSlot].insert(std::make_pair(bindPoint.slotNumber, resource));
		if (insertResultL.second == false)
		{
#if defined(USE_CRY_ASSERT)
			auto& existingResource = insertResultL.first->second;
			auto& currentResource = resource;

			CRY_ASSERT_TRACE(false, ("Invalid Resource Layout : Multiple resources bound to shader slot %s within the same layoutSlot: A: %s - B: %s",
				GetBindPointName(bindPoint), GetResourceName(existingResource), GetResourceName(currentResource)));
#endif

			return false;
		}

		return true;
	};

	// validate all resource bindings
	for (auto& itLayoutBinding : layoutDesc.m_resourceBindings)
	{
		if (!validateLayoutSlot(itLayoutBinding.first.layoutSlot))
			return false;

		// Call validate resource set description
		if (!ValidateResourceSet(itLayoutBinding.second))
			return false;

		for (auto itResource : itLayoutBinding.second)
		{
			if (!validateResourceBindPoint(itLayoutBinding.first, itResource.first, itResource.second))
				return false;
		}
	}

	///////////////////////////////////////////// Stage 2: validate against device limits /////////////////////////////////////////////
#define VALIDATE_LIMIT(requiredValue, supportedValue, errorMessage)                                          \
	if (requiredValue > supportedValue)                                                                      \
	{                                                                                                        \
		CRY_ASSERT_TRACE(false, ("Invalid Resource Layout: " errorMessage, requiredValue, supportedValue));  \
		return false;                                                                                        \
	}

	// Make sure there are no 'holes' in the used binding slots
	{
		perDrawResources.NumBindings = static_cast<uint32>(usedLayoutBindSlots.size());
		int previousSlot = -1;
		for (auto slot : usedLayoutBindSlots)
		{
			if (slot != previousSlot + 1)
			{
				CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: gap in layout (CPU side) slots");
				return false;
			}

			previousSlot = slot;
		}

		if (previousSlot > EResourceLayoutSlot_Max)
			return false;
	}

	// Check binding count
	const uint32 srvCount = perDrawResources.NumSRVs;
	const uint32 uavCount = perDrawResources.NumUAVs;

	// *INDENT-OFF*
	VALIDATE_LIMIT(perDrawResources.NumBindings,        Limits.PerDraw.NumBindings,                 "Binding count exceeded %d of %d");
	VALIDATE_LIMIT(perDrawResources.NumSamplers,        Limits.PerDraw.NumSamplers,                 "Per draw sampler count exceeded: %d of %d");
	VALIDATE_LIMIT(perDrawResources.NumConstantBuffers, Limits.PerDraw.NumConstantBuffers,          "Per draw constant buffer count exceeded: %d of %d");
	VALIDATE_LIMIT(perDrawResources.NumBuffers,         Limits.PerDraw.NumBuffers,                  "Per draw buffer count exceeded: %d of %d");
	VALIDATE_LIMIT(perDrawResources.NumBufferSRVs,      Limits.PerDraw.NumBufferSRVs,               "Per draw buffer SRV count exceeded: %d of %d");
	VALIDATE_LIMIT(perDrawResources.NumBufferUAVs,      Limits.PerDraw.NumBufferSRVs,               "Per draw buffer UAV count exceeded: %d of %d");
	VALIDATE_LIMIT(perDrawResources.NumTextures,        Limits.PerDraw.NumTextures,                 "Per draw texture count exceeded: %d of %d");
	VALIDATE_LIMIT(perDrawResources.NumTextureSRVs,     Limits.PerDraw.NumTextureSRVs,              "Per draw texture SRV count exceeded: %d of %d");
	VALIDATE_LIMIT(perDrawResources.NumTextureUAVs,     Limits.PerDraw.NumTextureUAVs,              "Per draw texture UAV count exceeded: %d of %d");
	VALIDATE_LIMIT(srvCount,                            Limits.PerDraw.NumSRVs,                     "Per draw SRV count exceeded: %d of %d");
	VALIDATE_LIMIT(uavCount,                            Limits.PerDraw.NumUAVs,                     "Per draw UAV count exceeded: %d of %d");
	// *INDENT-ON*

	// check per stage resources
	{
		for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
		{
			const auto& stageResources = perStageResources[shaderClass];
			const uint32 srvCount = stageResources.NumTextureSRVs + stageResources.NumBufferSRVs;
			const uint32 uavCount = stageResources.NumTextureUAVs + stageResources.NumBufferUAVs;
			const uint32 resourceCount = srvCount + uavCount + stageResources.NumConstantBuffers + stageResources.NumSamplers;

			// *INDENT-OFF*
			VALIDATE_LIMIT(stageResources.NumSamplers,        Limits.PerDraw.PerShaderStage.NumSamplers,        "Per stage sampler count exceeded: %d of %d");
			VALIDATE_LIMIT(stageResources.NumConstantBuffers, Limits.PerDraw.PerShaderStage.NumConstantBuffers, "Per stage constant buffer count exceeded: %d of %d");
			VALIDATE_LIMIT(stageResources.NumBuffers,         Limits.PerDraw.PerShaderStage.NumBuffers,         "Per stage buffer count exceeded: %d of %d");
			VALIDATE_LIMIT(stageResources.NumBufferSRVs,      Limits.PerDraw.PerShaderStage.NumBufferSRVs,      "Per stage buffer SRV count exceeded: %d of %d");
			VALIDATE_LIMIT(stageResources.NumBufferUAVs,      Limits.PerDraw.PerShaderStage.NumBufferSRVs,      "Per stage buffer UAV count exceeded: %d of %d");
			VALIDATE_LIMIT(stageResources.NumTextures,        Limits.PerDraw.PerShaderStage.NumTextures,        "Per stage texture count exceeded: %d of %d");
			VALIDATE_LIMIT(stageResources.NumTextureSRVs,     Limits.PerDraw.PerShaderStage.NumTextureSRVs,     "Per stage texture SRV count exceeded: %d of %d");
			VALIDATE_LIMIT(stageResources.NumTextureUAVs,     Limits.PerDraw.PerShaderStage.NumTextureUAVs,     "Per stage texture UAV count exceeded: %d of %d");
			VALIDATE_LIMIT(srvCount,                          Limits.PerDraw.PerShaderStage.NumSRVs,            "Per stage SRV count exceeded: %d of %d");
			VALIDATE_LIMIT(uavCount,                          Limits.PerDraw.PerShaderStage.NumUAVs,            "Per stage UAV count exceeded: %d of %d");
			VALIDATE_LIMIT(resourceCount,                     Limits.PerDraw.PerShaderStage.NumShaderResources, "Per stage resource count exceeded: %d of %d");
			// *INDENT-ON*
		}
	}

#undef VALIDATE_LIMIT

	return true;
}

bool CDeviceObjectValidator::ValidateResourceSet(const VectorMap<SResourceBindPoint, SResourceBinding>& resourceBindings) const
{
	uint32_t bufferCount = 0;
	for (const auto& binding : resourceBindings)
	{
		if (binding.second.type == SResourceBinding::EResourceType::Buffer)
			++bufferCount;
	}

	if (bufferCount > Limits.PerResourceSet.NumBuffers)
	{
		CRY_ASSERT_MESSAGE(false, "Invalid Resource set: buffer count exceeded: %d of %d", bufferCount, Limits.PerResourceSet.NumBuffers);
		return false;
	}

	return true;
}

