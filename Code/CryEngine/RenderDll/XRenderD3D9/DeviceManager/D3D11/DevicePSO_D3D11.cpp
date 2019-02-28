// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../DeviceObjects.h"
#include "DriverD3D.h"
#include "../../../Common/ReverseDepth.h"

#include "DeviceResourceSet_D3D11.h"
#include "DevicePSO_D3D11.h"
#include "DeviceObjects_D3D11.h"

CDeviceGraphicsPSO_DX11::CDeviceGraphicsPSO_DX11()
{
	m_PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_ShaderFlags_RT = 0;
	m_ShaderFlags_MD = 0;
	m_ShaderFlags_MDV = 0;
	m_RasterizerStateIndex = 0;

	m_NumSamplers.fill(0);
	m_NumSRVs.fill(0);
}

CDeviceGraphicsPSO::EInitResult CDeviceGraphicsPSO_DX11::Init(const CDeviceGraphicsPSODesc& psoDesc)
{
	CD3D9Renderer* rd = gcpRendD3D;
	EInitResult result = EInitResult::Success;
	m_isValid = false;
	m_updateCount++;

	m_pRasterizerState = nullptr;
	m_pBlendState = nullptr;
	m_pDepthStencilState = nullptr;
	m_pInputLayout = nullptr;

	m_NumSamplers.fill(0);
	m_NumSRVs.fill(0);

	SDeviceObjectHelpers::THwShaderInfo hwShaders;
	EShaderStage validShaderStages = SDeviceObjectHelpers::GetShaderInstanceInfo(hwShaders, psoDesc.m_pShader, psoDesc.m_technique,
		psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, psoDesc.m_bAllowTesselation);

	if (validShaderStages == EShaderStage_None)
		return EInitResult::Failure;

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_NumGfx; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		m_pDeviceShaders[shaderClass]     = hwShaders[shaderClass].pDeviceShader;
		m_pHwShaderInstances[shaderClass] = hwShaders[shaderClass].pHwShaderInstance;
	}

	D3D11_RASTERIZER_DESC rasterizerDesc;
	D3D11_BLEND_DESC blendDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;

	psoDesc.FillDescs(rasterizerDesc, blendDesc, depthStencilDesc);

	uint32 rasterStateIndex = CDeviceStatesManagerDX11::GetInstance()->GetOrCreateRasterState(rasterizerDesc);
	uint32 blendStateIndex = CDeviceStatesManagerDX11::GetInstance()->GetOrCreateBlendState(blendDesc);
	uint32 depthStateIndex = CDeviceStatesManagerDX11::GetInstance()->GetOrCreateDepthState(depthStencilDesc);

	if (rasterStateIndex == uint32(-1) || blendStateIndex == uint32(-1) || depthStateIndex == uint32(-1))
		return EInitResult::Failure;

	m_pDepthStencilState = CDeviceStatesManagerDX11::GetInstance()->m_StatesDP[depthStateIndex].pState;
	m_pRasterizerState = CDeviceStatesManagerDX11::GetInstance()->m_StatesRS[rasterStateIndex].pState;
	m_RasterizerStateIndex = rasterStateIndex;
	m_pBlendState = CDeviceStatesManagerDX11::GetInstance()->m_StatesBL[blendStateIndex].pState;

	// input layout
	m_pInputLayout = nullptr;
	if (psoDesc.m_VertexFormat != EDefaultInputLayouts::Empty)
	{
		if (psoDesc.m_VertexFormat != EDefaultInputLayouts::Unspecified)
		{
			if (CHWShader_D3D::SHWSInstance* pVsInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(hwShaders[eHWSC_Vertex].pHwShaderInstance))
			{
				uint32 streamMask = psoDesc.CombineVertexStreamMasks(uint8(pVsInstance->m_VStreamMask_Decl), psoDesc.m_ObjectStreamMask);

				const auto inputLayoutPair = CDeviceObjectFactory::GetOrCreateInputLayout(&pVsInstance->m_Shader, streamMask, psoDesc.m_VertexFormat);
				m_pInputLayout = inputLayoutPair->second;
			}
		}

		if (!m_pInputLayout)
			return EInitResult::Failure;
	}

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_NumGfx; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (auto pShaderInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(hwShaders[shaderClass].pHwShaderInstance))
		{
			for (const auto& smp : pShaderInstance->m_Samplers)
				m_Samplers[shaderClass][m_NumSamplers[shaderClass]++] = uint8(smp.m_dwCBufSlot);

			for (const auto& tex : pShaderInstance->m_Textures)
				m_SRVs[shaderClass][m_NumSRVs[shaderClass]++] = uint8(tex.m_dwCBufSlot);
		}
	}

	if (!ValidateShadersAndTopologyCombination(psoDesc, m_pHwShaderInstances))
		return EInitResult::ErrorShadersAndTopologyCombination;

	m_PrimitiveTopology = static_cast<D3DPrimitiveType>(psoDesc.m_PrimitiveType);

	m_ShaderFlags_RT = psoDesc.m_ShaderFlags_RT;
	m_ShaderFlags_MD = psoDesc.m_ShaderFlags_MD;
	m_ShaderFlags_MDV = psoDesc.m_ShaderFlags_MDV;

#if defined(ENABLE_PROFILING_CODE)
	m_PrimitiveTypeForProfiling = psoDesc.m_PrimitiveType;
#endif

	m_isValid = true;
	return EInitResult::Success;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceComputePSO_DX11::CDeviceComputePSO_DX11()
{
}

bool CDeviceComputePSO_DX11::Init(const CDeviceComputePSODesc& psoDesc)
{
	CD3D9Renderer* rd = gcpRendD3D;
	m_isValid = false;
	m_updateCount++;

	SDeviceObjectHelpers::THwShaderInfo hwShaders;
	EShaderStage validShaderStages = SDeviceObjectHelpers::GetShaderInstanceInfo(hwShaders, psoDesc.m_pShader, psoDesc.m_technique, 
		psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, false);

	if (validShaderStages != EShaderStage_Compute)
		return false;

	m_pDeviceShaders[eHWSC_Compute] = hwShaders[eHWSC_Compute].pDeviceShader;
	m_pHwShaderInstance             = hwShaders[eHWSC_Compute].pHwShaderInstance;

	m_isValid = true;
	return true;
}
