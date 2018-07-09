// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKShader.hpp"
#include "VkSpirvHelper.hpp"


namespace NCryVulkan
{

	CShader::CShader(CDevice* pDevice)
		: CDeviceObject(pDevice)
		, m_shaderHandle(VK_NULL_HANDLE)
	{}
	
	CShader::~CShader()
	{
		vkDestroyShaderModule(GetDevice()->GetVkDevice(), m_shaderHandle, nullptr);
	}

	VkResult CShader::Init(const void* pShaderCode, size_t shaderCodeSize)
	{
		bool stripReflections = false;
		if (CRendererCVars::CV_r_VkShaderCompiler && strcmp(CRendererCVars::CV_r_VkShaderCompiler->GetString(), STR_VK_SHADER_COMPILER_GLSLANG) == 0)
			stripReflections = true;

		std::vector<uint32> shaderCode((shaderCodeSize + 3) / 4);
		memcpy(shaderCode.data(), pShaderCode, shaderCodeSize);
		if (stripReflections)
		{
			bool success = VkSpvHelper::StripGoogleExtensionsFromShader(shaderCode);
			CRY_ASSERT(success == true);
			shaderCodeSize = static_cast<uint32_t>(shaderCode.size());
		}

		CRY_ASSERT(shaderCodeSize > 0);

		VkShaderModuleCreateInfo shaderCreateInfo = {};
		shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderCreateInfo.flags = 0;
		shaderCreateInfo.pCode = reinterpret_cast<const uint32*>(shaderCode.data());
		shaderCreateInfo.codeSize = shaderCodeSize;
		shaderCreateInfo.pNext = nullptr;

		return vkCreateShaderModule(GetDevice()->GetVkDevice(), &shaderCreateInfo, nullptr, &m_shaderHandle);
	}
}
