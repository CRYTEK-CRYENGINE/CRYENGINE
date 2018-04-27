// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12Shader.hpp"

#if CRY_PLATFORM_DURANGO
#include <d3d11shader_x.h>
#include <d3dcompiler_x.h>
#else
#include <d3d11shader.h>
#include <d3dcompiler.h>
#endif

CCryDX12Shader* CCryDX12Shader::Create(CCryDX12Device* pDevice, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage)
{
	CCryDX12Shader* pResult = DX12_NEW_RAW(CCryDX12Shader(pShaderBytecode, BytecodeLength));
	pResult->m_pShader = NCryDX12::CShader::CreateFromD3D11(pDevice->GetDX12Device(), pResult->GetD3D12ShaderBytecode());

	return pResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12Shader::CCryDX12Shader(const void* pShaderBytecode, SIZE_T BytecodeLength)
	: Super(nullptr, nullptr)
	, m_pShaderBytecodeData(NULL)
{
	if (pShaderBytecode && BytecodeLength)
	{
		m_pShaderBytecodeData = new uint8_t[BytecodeLength];
		memcpy(m_pShaderBytecodeData, pShaderBytecode, BytecodeLength);

		m_ShaderBytecode12.pShaderBytecode = m_pShaderBytecodeData;
		m_ShaderBytecode12.BytecodeLength = BytecodeLength;
	}
	else
	{
		m_ShaderBytecode12.pShaderBytecode = NULL;
		m_ShaderBytecode12.BytecodeLength = 0;
	}
}

CCryDX12Shader::~CCryDX12Shader()
{
	if (m_pShaderBytecodeData)
	{
		delete[] m_pShaderBytecodeData;
	}
}
