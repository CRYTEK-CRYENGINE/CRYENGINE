#include "StdAfx.h"
#include "VKShader.hpp"

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
		VkShaderModuleCreateInfo shaderCreateInfo = {};
		shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderCreateInfo.flags = 0;
		shaderCreateInfo.pCode = reinterpret_cast<const uint32*>(pShaderCode);
		shaderCreateInfo.codeSize = shaderCodeSize;
		shaderCreateInfo.pNext = nullptr;

		return vkCreateShaderModule(GetDevice()->GetVkDevice(), &shaderCreateInfo, nullptr, &m_shaderHandle);
	}
}
