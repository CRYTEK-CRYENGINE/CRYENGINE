// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12Shader.hpp"

#if CRY_PLATFORM_DURANGO
#include <d3d11shader_x.h>
#include <d3dcompiler_x.h>
#else
#include <d3d11shader.h>
#include <d3dcompiler.h>
#endif

namespace NCryDX12
{

//---------------------------------------------------------------------------------------------------------------------
CShader* CShader::Create(CDevice* device, const D3D12_SHADER_BYTECODE& byteCode)
{
	ID3D12ShaderReflection* shaderReflection = NULL;

	// Reflect shader internals
	if (S_OK != D3DReflection(byteCode.pShaderBytecode, byteCode.BytecodeLength, IID_D3DShaderReflection, (void**)&shaderReflection))
	{
		DX12_ERROR("Could not do a shader reflection!");
		return NULL;
	}

	D3D12_SHADER_DESC desc12;
	shaderReflection->GetDesc(&desc12);

	CShader* result = DX12_NEW_RAW(CShader(device));
	result->m_Bytecode = byteCode;
	result->m_Hash = result->ConvertToHash();

	for (UINT i = 0; i < desc12.BoundResources; ++i)
	{
		D3D12_SHADER_INPUT_BIND_DESC bindDesc12;
		shaderReflection->GetResourceBindingDesc(i, &bindDesc12);
		bool bBindPointUsed = true;     // TODO: this is not indicating the correct thing: !!(bindDesc11.uFlags & D3D10_SVF_USED);
		bool bBindingSharable = false;  // TODO: the per-frame CBs for the different stages are NOT all equal: bindDesc11.BindPoint == CB_PER_FRAME || bindDesc11.BindPoint == CB_PER_SHADOWGEN
		bool bBindingUnmergable = true; //(bindDesc11.BindPoint == CB_PER_FRAME || bindDesc11.BindPoint == CB_PER_SHADOWGEN);

		switch (bindDesc12.Type)
		{
		case D3D10_SIT_CBUFFER:
			DX12_ASSERT(bindDesc12.BindCount == 1, "Arrays of ConstantBuffers are not allowed!");

			if (bindDesc12.BindCount == 1)
			{
				ID3D12ShaderReflectionConstantBuffer* pCB = shaderReflection->GetConstantBufferByName(bindDesc12.Name);
				D3D12_SHADER_BUFFER_DESC sSBDesc;
				UINT nUsed = 0;

				pCB->GetDesc(&sSBDesc);
				for (UINT j = 0; j < sSBDesc.Variables; j++)
				{
					ID3D12ShaderReflectionVariable* pCV = pCB->GetVariableByIndex(j);
					D3D12_SHADER_VARIABLE_DESC CDesc;
					pCV->GetDesc(&CDesc);

					nUsed += (CDesc.uFlags & D3D10_SVF_USED);
				}

				bBindPointUsed = (nUsed > 0);
			}

			// We need separate descriptor tables for dynamic CB's
			result->m_Ranges.m_ConstantBuffers.Add(bindDesc12.BindPoint, bindDesc12.BindCount, static_cast<UINT8>(bindDesc12.Type), static_cast<UINT8>(bindDesc12.Dimension), bBindPointUsed, bBindingSharable, bBindingUnmergable);
			break;

		// ID3D12Device::CreateGraphicsPipelineState: SRV or UAV root descriptors can only be Raw or Structured buffers.
		case D3D10_SIT_TEXTURE:
		case D3D10_SIT_TBUFFER:
			result->m_Ranges.m_InputResources.Add(bindDesc12.BindPoint, bindDesc12.BindCount, static_cast<UINT8>(bindDesc12.Type), static_cast<UINT8>(bindDesc12.Dimension));
			break;
		case D3D11_SIT_STRUCTURED:
		case D3D11_SIT_BYTEADDRESS:
			result->m_Ranges.m_InputResources.Add(bindDesc12.BindPoint, bindDesc12.BindCount, static_cast<UINT8>(bindDesc12.Type), static_cast<UINT8>(bindDesc12.Dimension), bBindPointUsed);
			break;

		case D3D11_SIT_UAV_RWTYPED:
			result->m_Ranges.m_OutputResources.Add(bindDesc12.BindPoint, bindDesc12.BindCount, static_cast<UINT8>(bindDesc12.Type), static_cast<UINT8>(bindDesc12.Dimension));
			break;
		case D3D11_SIT_UAV_RWSTRUCTURED:
		case D3D11_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
		case D3D11_SIT_UAV_RWBYTEADDRESS:
		case D3D11_SIT_UAV_APPEND_STRUCTURED:
		case D3D11_SIT_UAV_CONSUME_STRUCTURED:
			result->m_Ranges.m_OutputResources.Add(bindDesc12.BindPoint, bindDesc12.BindCount, static_cast<UINT8>(bindDesc12.Type), static_cast<UINT8>(bindDesc12.Dimension), bBindPointUsed);
			break;

		case D3D10_SIT_SAMPLER:
			DX12_ASSERT(bindDesc12.BindCount == 1, "Arrays of SamplerStates are not allowed!");

			result->m_Ranges.m_Samplers.Add(bindDesc12.BindPoint, bindDesc12.BindCount, static_cast<UINT8>(bindDesc12.Type), static_cast<UINT8>(bindDesc12.Dimension));
			break;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
CShader::CShader(CDevice* device)
	: CDeviceObject(device)
{

}

//---------------------------------------------------------------------------------------------------------------------
CShader::~CShader()
{

}

}
