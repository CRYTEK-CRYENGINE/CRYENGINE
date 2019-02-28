// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "DeviceResourceSet_D3D12.h"	
#include "DevicePSO_D3D12.h"

static struct
{
	ERenderPrimitiveType          primitiveType;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology;
}
topologyTypes[] =
{
	{ eptUnknown,                D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED },
	{ eptTriangleList,           D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE },
	{ eptTriangleStrip,          D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE },
	{ eptLineList,               D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE },
	{ eptLineStrip,              D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE },
	{ eptPointList,              D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT },
	{ ept1ControlPointPatchList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH },
	{ ept2ControlPointPatchList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH },
	{ ept3ControlPointPatchList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH },
	{ ept4ControlPointPatchList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH },
};

CDeviceGraphicsPSO::EInitResult CDeviceGraphicsPSO_DX12::Init(const CDeviceGraphicsPSODesc& psoDesc)
{
	m_isValid = false;
	m_updateCount++;

	if (psoDesc.m_pResourceLayout == NULL)
		return EInitResult::Failure;

	const CDeviceRenderPassDesc* pRenderPassDesc = GetDeviceObjectFactory().GetRenderPassDesc(psoDesc.m_pRenderPass.get());
	if (!pRenderPassDesc)
		return EInitResult::Failure;

	SDeviceObjectHelpers::THwShaderInfo hwShaders;
	::EShaderStage validShaderStages = SDeviceObjectHelpers::GetShaderInstanceInfo(hwShaders, psoDesc.m_pShader, psoDesc.m_technique, 
		psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, psoDesc.m_bAllowTesselation);

	if (validShaderStages == EShaderStage_None)
		return CDeviceGraphicsPSO::EInitResult::Failure;

	// validate shaders first
	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_NumGfx; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		m_pHwShaderInstances[shaderClass] = hwShaders[shaderClass].pHwShaderInstance;
	}

	D3D11_RASTERIZER_DESC rasterizerDesc;
	D3D11_BLEND_DESC blendDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	psoDesc.FillDescs(rasterizerDesc, blendDesc, depthStencilDesc);

	// prepare pso init params
	CGraphicsPSO::SInitParams psoInitParams;
	ZeroStruct(psoInitParams);

	// root signature
	psoInitParams.m_Desc.pRootSignature = reinterpret_cast<CDeviceResourceLayout_DX12*>(psoDesc.m_pResourceLayout.get())->GetRootSignature()->GetD3D12RootSignature();

	// rendertarget formats
	{

		const auto& renderTargets = pRenderPassDesc->GetRenderTargets();

		for (int i = 0; i < renderTargets.size(); ++i)
		{
			if (!renderTargets[i].pTexture)
				break;

			psoInitParams.m_Desc.RTVFormats[psoInitParams.m_Desc.NumRenderTargets++] = renderTargets[i].GetResourceFormat();
		}

		if (pRenderPassDesc->GetDepthTarget().pTexture)
			psoInitParams.m_Desc.DSVFormat = pRenderPassDesc->GetDepthTarget().GetResourceFormat();
	}

	// blend state
	{
		psoInitParams.m_Desc.BlendState.AlphaToCoverageEnable = blendDesc.AlphaToCoverageEnable;
		psoInitParams.m_Desc.BlendState.IndependentBlendEnable = blendDesc.IndependentBlendEnable;

		for (int i = 0; i < CRY_ARRAY_COUNT(blendDesc.RenderTarget); ++i)
		{
			uint32 RenderTargetWriteMask = i < psoInitParams.m_Desc.NumRenderTargets ? DeviceFormats::GetWriteMask(psoInitParams.m_Desc.RTVFormats[i]) : 0;

			psoInitParams.m_Desc.BlendState.RenderTarget[i].BlendEnable = blendDesc.RenderTarget[i].BlendEnable;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].LogicOpEnable = false;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].SrcBlend = (D3D12_BLEND)blendDesc.RenderTarget[i].SrcBlend;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].DestBlend = (D3D12_BLEND)blendDesc.RenderTarget[i].DestBlend;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].BlendOp = (D3D12_BLEND_OP)blendDesc.RenderTarget[i].BlendOp;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].SrcBlendAlpha = (D3D12_BLEND)blendDesc.RenderTarget[i].SrcBlendAlpha;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].DestBlendAlpha = (D3D12_BLEND)blendDesc.RenderTarget[i].DestBlendAlpha;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].BlendOpAlpha = (D3D12_BLEND_OP)blendDesc.RenderTarget[i].BlendOpAlpha;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].RenderTargetWriteMask = blendDesc.RenderTarget[i].RenderTargetWriteMask & RenderTargetWriteMask;
		}
	}

	// depth stencil and rasterizer state
	memcpy(&psoInitParams.m_Desc.DepthStencilState, &depthStencilDesc, sizeof(depthStencilDesc));
	memcpy(&psoInitParams.m_Desc.RasterizerState, &rasterizerDesc, sizeof(rasterizerDesc));
	psoInitParams.m_DepthBoundsTestEnable = psoDesc.m_bDepthBoundsTest;

	auto extractShaderBytecode = [&](EHWShaderClass shaderClass)
	{
		if (hwShaders[shaderClass].pHwShader)
		{
			return reinterpret_cast<CCryDX12Shader*>(hwShaders[shaderClass].pDeviceShader)->GetD3D12ShaderBytecode();
		}
		else
		{
			D3D12_SHADER_BYTECODE emptyShader = { NULL, 0 };
			return emptyShader;
		}
	};

	psoInitParams.m_Desc.NodeMask = m_pDevice->GetNodeMask();
	psoInitParams.m_Desc.VS = extractShaderBytecode(eHWSC_Vertex);
	psoInitParams.m_Desc.PS = extractShaderBytecode(eHWSC_Pixel);
	psoInitParams.m_Desc.GS = extractShaderBytecode(eHWSC_Geometry);
	psoInitParams.m_Desc.DS = extractShaderBytecode(eHWSC_Domain);
	psoInitParams.m_Desc.HS = extractShaderBytecode(eHWSC_Hull);

	psoInitParams.m_Desc.SampleMask = UINT_MAX;
	psoInitParams.m_Desc.SampleDesc.Count = 1;
	// primitive topology
	{
		if (!ValidateShadersAndTopologyCombination(psoDesc, m_pHwShaderInstances))
			return EInitResult::ErrorShadersAndTopologyCombination;

		m_PrimitiveTopology = static_cast<D3DPrimitiveType>(psoDesc.m_PrimitiveType);

		psoInitParams.m_Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		for (int i = 0; i < CRY_ARRAY_COUNT(topologyTypes); ++i)
		{
			if (topologyTypes[i].primitiveType == psoDesc.m_PrimitiveType)
			{
				psoInitParams.m_Desc.PrimitiveTopologyType = topologyTypes[i].primitiveTopology;
				break;
			}
		}
	}

	// input layout
	if (psoDesc.m_VertexFormat != EDefaultInputLayouts::Empty)
	{
		if (psoDesc.m_VertexFormat == EDefaultInputLayouts::Unspecified)
			return EInitResult::Failure;

		if (CHWShader_D3D::SHWSInstance* pVsInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(hwShaders[eHWSC_Vertex].pHwShaderInstance))
		{
			uint32 streamMask = psoDesc.CombineVertexStreamMasks(uint8(pVsInstance->m_VStreamMask_Decl), psoDesc.m_ObjectStreamMask);

			const auto inputLayoutPair = CDeviceObjectFactory::GetOrCreateInputLayout(&pVsInstance->m_Shader, streamMask, psoDesc.m_VertexFormat);
			m_pInputLayout = &inputLayoutPair->first;

			// D3D12_INPUT_ELEMENT_DESC := D3D11_INPUT_ELEMENT_DESC
			psoInitParams.m_Desc.InputLayout.pInputElementDescs = reinterpret_cast<const D3D12_INPUT_ELEMENT_DESC*>(&m_pInputLayout->m_Declaration[0]);
			psoInitParams.m_Desc.InputLayout.NumElements = m_pInputLayout->m_Declaration.size();
		}
	}

#if defined(ENABLE_PROFILING_CODE)
	m_PrimitiveTypeForProfiling = psoDesc.m_PrimitiveType;
#endif

	m_pDevice->GetPSOCache().GetOrCreatePSO(psoInitParams, m_pGraphicsPSO);

	m_isValid = (m_pGraphicsPSO != nullptr);
	return (m_isValid ? EInitResult::Success : EInitResult::Failure);
}

bool CDeviceComputePSO_DX12::Init(const CDeviceComputePSODesc& psoDesc)
{
	m_isValid = false;
	m_updateCount++;

	if (psoDesc.m_pResourceLayout == NULL)
		return false;

	SDeviceObjectHelpers::THwShaderInfo hwShaders;
	::EShaderStage validShaderStages = SDeviceObjectHelpers::GetShaderInstanceInfo(hwShaders, psoDesc.m_pShader, psoDesc.m_technique, 
		psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, false);

	if (validShaderStages != EShaderStage_Compute)
		return false;

	m_pHwShaderInstance = hwShaders[eHWSC_Compute].pHwShaderInstance;

	// prepare pso init params
	CComputePSO::SInitParams psoInitParams;
	ZeroStruct(psoInitParams);

	// root signature
	psoInitParams.m_Desc.pRootSignature = reinterpret_cast<CDeviceResourceLayout_DX12*>(psoDesc.m_pResourceLayout.get())->GetRootSignature()->GetD3D12RootSignature();
	psoInitParams.m_Desc.NodeMask = m_pDevice->GetNodeMask();
	if (hwShaders[eHWSC_Compute].pHwShader)
	{
		psoInitParams.m_Desc.CS = static_cast<CCryDX12Shader*>(hwShaders[eHWSC_Compute].pDeviceShader)->GetD3D12ShaderBytecode();
	}

	m_pDevice->GetPSOCache().GetOrCreatePSO(psoInitParams, m_pComputePSO);

	return (m_isValid = (m_pComputePSO != nullptr));
}
