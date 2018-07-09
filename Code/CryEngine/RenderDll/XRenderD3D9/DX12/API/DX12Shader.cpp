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
CShader* CShader::CreateFromD3D11(CDevice* device, const D3D12_SHADER_BYTECODE& byteCode)
{
	ID3D11ShaderReflection* shaderReflection = NULL;

	// Reflect shader internals
	if (S_OK != D3DReflect(byteCode.pShaderBytecode, byteCode.BytecodeLength, IID_ID3D11ShaderReflection, (void**)&shaderReflection))
	{
		DX12_ASSERT(0, "Could not do a shader reflection!");
		return NULL;
	}

	D3D11_SHADER_DESC desc11;
	shaderReflection->GetDesc(&desc11);

	CShader* result = DX12_NEW_RAW(CShader(device));
	result->m_Bytecode = byteCode;
	result->m_Hash = result->ConvertToHash();

	for (UINT i = 0; i < desc11.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC bindDesc11;
		shaderReflection->GetResourceBindingDesc(i, &bindDesc11);
		bool bBindPointUsed = true;     // TODO: this is not indicating the correct thing: !!(bindDesc11.uFlags & D3D10_SVF_USED);
		bool bBindingSharable = false;  // TODO: the per-frame CBs for the different stages are NOT all equal: bindDesc11.BindPoint == CB_PER_FRAME || bindDesc11.BindPoint == CB_PER_SHADOWGEN
		bool bBindingUnmergable = true; //(bindDesc11.BindPoint == CB_PER_FRAME || bindDesc11.BindPoint == CB_PER_SHADOWGEN);

		switch (bindDesc11.Type)
		{
		case D3D10_SIT_CBUFFER:
			DX12_ASSERT(bindDesc11.BindCount == 1, "Arrays of ConstantBuffers are not allowed!");

			if (bindDesc11.BindCount == 1)
			{
				ID3D11ShaderReflectionConstantBuffer* pCB = shaderReflection->GetConstantBufferByName(bindDesc11.Name);
				D3D11_SHADER_BUFFER_DESC sSBDesc;
				UINT nUsed = 0;

				pCB->GetDesc(&sSBDesc);
				for (UINT j = 0; j < sSBDesc.Variables; j++)
				{
					ID3D11ShaderReflectionVariable* pCV = pCB->GetVariableByIndex(j);
					D3D11_SHADER_VARIABLE_DESC CDesc;
					pCV->GetDesc(&CDesc);

					nUsed += (CDesc.uFlags & D3D10_SVF_USED);
				}

				bBindPointUsed = (nUsed > 0);
			}

			// We need separate descriptor tables for dynamic CB's
			result->m_Ranges.m_ConstantBuffers.Add(bindDesc11.BindPoint, bindDesc11.BindCount, static_cast<UINT8>(bindDesc11.Type), static_cast<UINT8>(bindDesc11.Dimension), bBindPointUsed, bBindingSharable, bBindingUnmergable);
			break;

		// ID3D12Device::CreateGraphicsPipelineState: SRV or UAV root descriptors can only be Raw or Structured buffers.
		case D3D10_SIT_TEXTURE:
		case D3D10_SIT_TBUFFER:
			result->m_Ranges.m_InputResources.Add(bindDesc11.BindPoint, bindDesc11.BindCount, static_cast<UINT8>(bindDesc11.Type), static_cast<UINT8>(bindDesc11.Dimension));
			break;
		case D3D11_SIT_STRUCTURED:
		case D3D11_SIT_BYTEADDRESS:
			result->m_Ranges.m_InputResources.Add(bindDesc11.BindPoint, bindDesc11.BindCount, static_cast<UINT8>(bindDesc11.Type), static_cast<UINT8>(bindDesc11.Dimension), bBindPointUsed);
			break;

		case D3D11_SIT_UAV_RWTYPED:
			result->m_Ranges.m_OutputResources.Add(bindDesc11.BindPoint, bindDesc11.BindCount, static_cast<UINT8>(bindDesc11.Type), static_cast<UINT8>(bindDesc11.Dimension));
			break;
		case D3D11_SIT_UAV_RWSTRUCTURED:
		case D3D11_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
		case D3D11_SIT_UAV_RWBYTEADDRESS:
		case D3D11_SIT_UAV_APPEND_STRUCTURED:
		case D3D11_SIT_UAV_CONSUME_STRUCTURED:
			result->m_Ranges.m_OutputResources.Add(bindDesc11.BindPoint, bindDesc11.BindCount, static_cast<UINT8>(bindDesc11.Type), static_cast<UINT8>(bindDesc11.Dimension), bBindPointUsed);
			break;

		case D3D10_SIT_SAMPLER:
			DX12_ASSERT(bindDesc11.BindCount == 1, "Arrays of SamplerStates are not allowed!");

			result->m_Ranges.m_Samplers.Add(bindDesc11.BindPoint, bindDesc11.BindCount, static_cast<UINT8>(bindDesc11.Type), static_cast<UINT8>(bindDesc11.Dimension));
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
