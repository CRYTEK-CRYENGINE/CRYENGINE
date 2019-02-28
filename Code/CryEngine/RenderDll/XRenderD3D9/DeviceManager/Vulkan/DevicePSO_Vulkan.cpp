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
#include "DevicePSO_Vulkan.h"	

using namespace NCryVulkan;


CDeviceGraphicsPSO_Vulkan::~CDeviceGraphicsPSO_Vulkan()
{
	m_pDevice->DeferDestruction(m_pipeline);
}

static struct
{
	ERenderPrimitiveType primitiveType;
	VkPrimitiveTopology  primitiveTopology;
	int                  controlPointCount;
}
topologyTypes[] =
{
	{ eptUnknown,                VK_PRIMITIVE_TOPOLOGY_MAX_ENUM, 0 },
	{ eptTriangleList,           VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0 },
	{ eptTriangleStrip,          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0 },
	{ eptLineList,               VK_PRIMITIVE_TOPOLOGY_LINE_LIST, 0 },
	{ eptLineStrip,              VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, 0 },
	{ eptPointList,              VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0 },
	{ ept1ControlPointPatchList, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 1 },
	{ ept2ControlPointPatchList, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 2 },
	{ ept3ControlPointPatchList, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 3 },
	{ ept4ControlPointPatchList, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 4 },
};

CDeviceGraphicsPSO::EInitResult CDeviceGraphicsPSO_Vulkan::Init(const CDeviceGraphicsPSODesc& psoDesc)
{
	m_isValid = false;
	m_updateCount++;

	if (psoDesc.m_pResourceLayout == nullptr || psoDesc.m_pShader == nullptr)
		return EInitResult::Failure;

	uint64 resourceLayoutHash = reinterpret_cast<CDeviceResourceLayout_Vulkan*>(psoDesc.m_pResourceLayout.get())->GetHash();
	UPipelineState customPipelineState[] = { resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash };

	SDeviceObjectHelpers::THwShaderInfo hwShaders;
	EShaderStage validShaderStages = SDeviceObjectHelpers::GetShaderInstanceInfo(hwShaders, psoDesc.m_pShader, psoDesc.m_technique, 
		psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, customPipelineState, psoDesc.m_bAllowTesselation);

	if (validShaderStages == EShaderStage_None)
		return EInitResult::Failure;

	// Vertex shader is required, both tessellation shaders should be used or omitted.
	if (hwShaders[eHWSC_Vertex].pHwShader == nullptr || (!(hwShaders[eHWSC_Domain].pHwShader == nullptr && hwShaders[eHWSC_Hull].pHwShader == nullptr) && !(hwShaders[eHWSC_Domain].pHwShader != nullptr && hwShaders[eHWSC_Hull].pHwShader != nullptr)))
		return EInitResult::Failure;

	const bool bDiscardRasterizer = (hwShaders[eHWSC_Pixel].pHwShader == nullptr) && (psoDesc.m_RenderState & GS_NODEPTHTEST) && !(psoDesc.m_RenderState & GS_DEPTHWRITE) && !(psoDesc.m_RenderState & GS_STENCIL);
	const bool bUsingDynamicDepthBias = psoDesc.m_bDynamicDepthBias;
	
	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[eHWSC_NumGfx];

	int validShaderCount = 0;
	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_NumGfx; shaderClass = EHWShaderClass(shaderClass + 1))
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
		if(CRendererCVars::CV_r_VkShaderCompiler && strcmp(CRendererCVars::CV_r_VkShaderCompiler->GetString(), STR_VK_SHADER_COMPILER_HLSLCC) == 0)
			shaderStageCreateInfos[validShaderCount].pName = "main"; // SPIR-V compiler (glslangValidator) currently supports this only
		else
			shaderStageCreateInfos[validShaderCount].pName = hwShaders[shaderClass].pHwShader->m_EntryFunc;
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

			uint32 streamMask = psoDesc.CombineVertexStreamMasks(uint8(pVsInstance->m_VStreamMask_Decl), psoDesc.m_ObjectStreamMask);

			const auto& inputLayoutPair = CDeviceObjectFactory::GetOrCreateInputLayout(&pVsInstance->m_Shader, streamMask, psoDesc.m_VertexFormat);
			const auto& inputLayout = inputLayoutPair->first;
			const unsigned int declarationCount = inputLayout.m_Declaration.size();

			// match shader inputs to input layout by semantic
			for (const auto& declInputElement : inputLayout.m_Declaration)
			{
				// update stride for this slot
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
				binding.stride = inputLayout.m_Strides[binding.binding - inputLayout.m_firstSlot];
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

	unsigned int controlPointCount = 0;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	// primitive topology
	{
		if (!ValidateShadersAndTopologyCombination(psoDesc, m_pHwShaderInstances))
			return EInitResult::ErrorShadersAndTopologyCombination;

		for (int i = 0; i < CRY_ARRAY_COUNT(topologyTypes); ++i)
		{
			if (topologyTypes[i].primitiveType == psoDesc.m_PrimitiveType)
			{
				topology = topologyTypes[i].primitiveTopology;
				controlPointCount = topologyTypes[i].controlPointCount;
				break;
			}
		}

		if (topology == VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)
		{
			CRY_ASSERT_MESSAGE(0, "Primitive type not supported");
			return EInitResult::Failure;
		}
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
	rasterizationStateCreateInfo.depthBiasEnable = bUsingDynamicDepthBias ? VK_TRUE : VK_FALSE;
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
	depthStencilStateCreateInfo.depthTestEnable = ((psoDesc.m_RenderState & GS_NODEPTHTEST) && !(psoDesc.m_RenderState & GS_DEPTHWRITE)) ? VK_FALSE : VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = (psoDesc.m_RenderState & GS_DEPTHWRITE) ? VK_TRUE : VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
	depthStencilStateCreateInfo.depthBoundsTestEnable = psoDesc.m_bDepthBoundsTest ? VK_FALSE : VK_TRUE;
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
		bool		  bAllowOnAllTargets;
	};

	static SBlendFactors SrcBlendFactors[GS_BLSRC_MASK >> GS_BLSRC_SHIFT] =
	{
		{ (VkBlendFactor)0,                    (VkBlendFactor)0,						true },		// UNINITIALIZED BLEND FACTOR
		{ VK_BLEND_FACTOR_ZERO,                VK_BLEND_FACTOR_ZERO,					true },		// GS_BLSRC_ZERO
		{ VK_BLEND_FACTOR_ONE,                 VK_BLEND_FACTOR_ONE,						true },		// GS_BLSRC_ONE
		{ VK_BLEND_FACTOR_DST_COLOR,           VK_BLEND_FACTOR_DST_ALPHA,				true },		// GS_BLSRC_DSTCOL
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,		true },		// GS_BLSRC_ONEMINUSDSTCOL
		{ VK_BLEND_FACTOR_SRC_ALPHA,           VK_BLEND_FACTOR_SRC_ALPHA,				true },		// GS_BLSRC_SRCALPHA
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,		true },		// GS_BLSRC_ONEMINUSSRCALPHA
		{ VK_BLEND_FACTOR_DST_ALPHA,           VK_BLEND_FACTOR_DST_ALPHA,				true },		// GS_BLSRC_DSTALPHA
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,		true },		// GS_BLSRC_ONEMINUSDSTALPHA
		{ VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,  VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,		true },		// GS_BLSRC_ALPHASATURATE
		{ VK_BLEND_FACTOR_SRC_ALPHA,           VK_BLEND_FACTOR_ZERO,					true },		// GS_BLSRC_SRCALPHA_A_ZERO
		{ VK_BLEND_FACTOR_SRC1_ALPHA,          VK_BLEND_FACTOR_SRC1_ALPHA,				false },	// GS_BLSRC_SRC1ALPHA
	};

	static SBlendFactors DstBlendFactors[GS_BLDST_MASK >> GS_BLDST_SHIFT] =
	{
		{ (VkBlendFactor)0,                     (VkBlendFactor)0,						true },		// UNINITIALIZED BLEND FACTOR
		{ VK_BLEND_FACTOR_ZERO,                 VK_BLEND_FACTOR_ZERO,					true },		// GS_BLDST_ZERO
		{ VK_BLEND_FACTOR_ONE,                  VK_BLEND_FACTOR_ONE,					true },		// GS_BLDST_ONE
		{ VK_BLEND_FACTOR_SRC_COLOR,            VK_BLEND_FACTOR_SRC_ALPHA ,				true },		// GS_BLDST_SRCCOL
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	true },		// GS_BLDST_ONEMINUSSRCCOL
		{ VK_BLEND_FACTOR_SRC_ALPHA,            VK_BLEND_FACTOR_SRC_ALPHA,				true },		// GS_BLDST_SRCALPHA
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	true },		// GS_BLDST_ONEMINUSSRCALPHA
		{ VK_BLEND_FACTOR_DST_ALPHA,            VK_BLEND_FACTOR_DST_ALPHA,				true },		// GS_BLDST_DSTALPHA
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,	true },		// GS_BLDST_ONEMINUSDSTALPHA
		{ VK_BLEND_FACTOR_ONE,                  VK_BLEND_FACTOR_ZERO,					true },		// GS_BLDST_ONE_A_ZERO
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,	false },	// GS_BLDST_ONEMINUSSRC1ALPHA
	};

	static VkBlendOp BlendOp[GS_BLEND_OP_MASK >> GS_BLEND_OP_SHIFT] =
	{
		VK_BLEND_OP_ADD,              // 0 (unspecified): Default
		VK_BLEND_OP_MAX,              // GS_BLOP_MAX / GS_BLALPHA_MAX
		VK_BLEND_OP_MIN,              // GS_BLOP_MIN / GS_BLALPHA_MIN
		VK_BLEND_OP_SUBTRACT,         // GS_BLOP_SUB / GS_BLALPHA_SUB
		VK_BLEND_OP_REVERSE_SUBTRACT, // GS_BLOP_SUBREV / GS_BLALPHA_SUBREV
	};

	VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[CD3D9Renderer::RT_STACK_WIDTH];

	int validBlendAttachmentStateCount = 0;

	const CDeviceRenderPassDesc* pRenderPassDesc = GetDeviceObjectFactory().GetRenderPassDesc(psoDesc.m_pRenderPass.get());
	if (!pRenderPassDesc)
		return EInitResult::Failure;

	const auto& renderTargets = pRenderPassDesc->GetRenderTargets();

	const bool bSrcBlendAllowAllTargets = SrcBlendFactors[(psoDesc.m_RenderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].bAllowOnAllTargets;
	const bool bDstBlendAllowAllTargets = DstBlendFactors[(psoDesc.m_RenderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].bAllowOnAllTargets;
	const bool bBlendAllowAllTargets = bSrcBlendAllowAllTargets && bDstBlendAllowAllTargets;

	for (int i = 0; i < renderTargets.size(); i++)
	{
		if (!renderTargets[i].pTexture)
			break;

		uint32_t colorWriteMask = 0;
		colorWriteMask = 0xfffffff0 | (ColorMasks[(psoDesc.m_RenderState & GS_COLMASK_MASK) >> GS_COLMASK_SHIFT][i]);
		colorWriteMask = (~colorWriteMask) & 0xf;

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

		// Dual source color blend cannot be enabled for any RT slot but 0
		if (!bBlendAllowAllTargets && i > 0)
		{
			colorBlendAttachmentStates[validBlendAttachmentStateCount].blendEnable = false;
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

	VkDynamicState dynamicStates[8];
	unsigned int dynamicStateCount = 0;

	dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
	dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
	dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS; // This must be enabled to not pick up the config for depth-bias in VkPipelineRasterizationStateCreateInfo (see above)
	if (psoDesc.m_bDepthBoundsTest)
		dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
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

	m_isValid = true;

	return EInitResult::Success;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceComputePSO_Vulkan::~CDeviceComputePSO_Vulkan()
{
	m_pDevice->DeferDestruction(m_pipeline);
}

bool CDeviceComputePSO_Vulkan::Init(const CDeviceComputePSODesc& psoDesc)
{
	m_isValid = false;
	m_updateCount++;

	if (psoDesc.m_pResourceLayout == nullptr || psoDesc.m_pShader == nullptr)
		return false;

	uint64 resourceLayoutHash = reinterpret_cast<CDeviceResourceLayout_Vulkan*>(psoDesc.m_pResourceLayout.get())->GetHash();
	UPipelineState customPipelineState[] = { resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash, resourceLayoutHash };

	SDeviceObjectHelpers::THwShaderInfo hwShaders;
	EShaderStage validShaderStages = SDeviceObjectHelpers::GetShaderInstanceInfo(hwShaders, psoDesc.m_pShader, psoDesc.m_technique,
		psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, customPipelineState, false);

	if (validShaderStages != EShaderStage_Compute)
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
	if(CRendererCVars::CV_r_VkShaderCompiler && strcmp(CRendererCVars::CV_r_VkShaderCompiler->GetString(), STR_VK_SHADER_COMPILER_HLSLCC) == 0)
		computePipelineCreateInfo.stage.pName = "main"; // SPIR-V compiler (glslangValidator) currently supports this only
	else
		computePipelineCreateInfo.stage.pName = hwShaders[eHWSC_Compute].pHwShader->m_EntryFunc; // SPIR-V compiler (glslangValidator) currently supports this only
	computePipelineCreateInfo.stage.pSpecializationInfo = nullptr;
	computePipelineCreateInfo.layout = static_cast<CDeviceResourceLayout_Vulkan*>(psoDesc.m_pResourceLayout.get())->GetVkPipelineLayout();
	computePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	computePipelineCreateInfo.basePipelineIndex = 0;

	VkResult result = vkCreateComputePipelines(m_pDevice->GetVkDevice(), m_pDevice->GetVkPipelineCache(), 1, &computePipelineCreateInfo, nullptr, &m_pipeline);

	if (result != VK_SUCCESS)
		return false;

	m_pHwShaderInstance = hwShaders[eHWSC_Compute].pHwShaderInstance;
	m_isValid = true;

	return true;
}
