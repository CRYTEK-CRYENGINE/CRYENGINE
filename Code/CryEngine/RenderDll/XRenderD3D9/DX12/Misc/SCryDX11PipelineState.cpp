// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SCryDX11PipelineState.hpp"

#include "DX12/API/DX12Shader.hpp"
#include "DX12/API/DX12RootSignature.hpp"
#include "DX12/Device/CCryDX12DeviceContext.hpp"


const D3D12_SHADER_BYTECODE& SCryDX11ShaderStageState::GetD3D12ShaderBytecode() const
{
	static D3D12_SHADER_BYTECODE emptyShader = { nullptr, 0 };
	return Shader.m_Value ? Shader.m_Value->GetD3D12ShaderBytecode() : emptyShader;
}

//---------------------------------------------------------------------------------------------------------------------
static const char* TopologyToString(D3D11_PRIMITIVE_TOPOLOGY type)
{
	switch (type)
	{
	case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
		return "point list";
	case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
		return "line list";
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
		return "line strip";
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
		return "triangle list";
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
		return "triangle strip";
	case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
		return "line list adj";
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
		return "line strip adj";
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
		return "triangle list adj";
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
		return "triangle strip adj";
	default:
		return "unmapped type";
	}
}

static const char* TypeToString(UINT8 dim)
{
	switch (dim)
	{
	case D3D_SIT_CBUFFER:
		return "constant buffer";
	case D3D_SIT_TBUFFER:
		return "texture buffer";
	case D3D_SIT_TEXTURE:
		return "texture";
	case D3D_SIT_SAMPLER:
		return "sampler";
	case D3D_SIT_UAV_RWTYPED:
		return "typed r/w texture";
	case D3D_SIT_STRUCTURED:
		return "structured buffer";
	case D3D_SIT_UAV_RWSTRUCTURED:
		return "structured r/w buffer";
	case D3D_SIT_BYTEADDRESS:
		return "raw buffer";
	case D3D_SIT_UAV_RWBYTEADDRESS:
		return "raw r/w buffer";
	case D3D_SIT_UAV_APPEND_STRUCTURED:
		return "append buffer";
	case D3D_SIT_UAV_CONSUME_STRUCTURED:
		return "consume buffer";
	case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
		return "structured r/w buffer with counter";
	default:
		return "unmapped dimension";
	}
}

static const char* DimensionToString(UINT8 dim)
{
	switch (dim)
	{
	case D3D_SRV_DIMENSION_UNKNOWN:
		return "unknown";
	case D3D_SRV_DIMENSION_BUFFER:
		return "buffer";
	case D3D_SRV_DIMENSION_TEXTURE1D:
		return "texture1d";
	case D3D_SRV_DIMENSION_TEXTURE1DARRAY:
		return "texture1d array";
	case D3D_SRV_DIMENSION_TEXTURE2D:
		return "texture2d";
	case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
		return "texture2d array";
	case D3D_SRV_DIMENSION_TEXTURE2DMS:
		return "texture2d ms";
	case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
		return "texture2d ms array";
	case D3D_SRV_DIMENSION_TEXTURE3D:
		return "texture3d";
	case D3D_SRV_DIMENSION_TEXTURECUBE:
		return "texturecube";
	case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
		return "texturecube array";
	case D3D_SRV_DIMENSION_BUFFEREX:
		return "buffer ex";
	default:
		return "unmapped dimension";
	}
}

static const char* DimensionToString(const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
	switch (desc.ViewDimension)
	{
	case D3D_SRV_DIMENSION_UNKNOWN:
		return "unknown";
	case D3D_SRV_DIMENSION_BUFFER:
		return desc.Buffer.Flags == D3D12_BUFFER_SRV_FLAG_RAW ? "raw buffer" : desc.Buffer.StructureByteStride > 0 ? "structured buffer" : "buffer";
	case D3D_SRV_DIMENSION_TEXTURE1D:
		return "texture1d";
	case D3D_SRV_DIMENSION_TEXTURE1DARRAY:
		return "texture1d array";
	case D3D_SRV_DIMENSION_TEXTURE2D:
		return "texture2d";
	case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
		return "texture2d array";
	case D3D_SRV_DIMENSION_TEXTURE2DMS:
		return "texture2d ms";
	case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
		return "texture2d ms array";
	case D3D_SRV_DIMENSION_TEXTURE3D:
		return "texture3d";
	case D3D_SRV_DIMENSION_TEXTURECUBE:
		return "texturecube";
	case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
		return "texturecube array";
	case D3D_SRV_DIMENSION_BUFFEREX:
		return desc.Buffer.Flags == D3D12_BUFFER_SRV_FLAG_RAW ? "raw buffer ex" : desc.Buffer.StructureByteStride > 0 ? "structured buffer" : "buffer ex";
	default:
		return "unmapped dimension";
	}
}

static const char* DimensionToString(const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
{
	switch (desc.ViewDimension)
	{
	case D3D_SRV_DIMENSION_UNKNOWN:
		return "r/w unknown";
	case D3D_SRV_DIMENSION_BUFFER:
		return desc.Buffer.Flags == D3D12_BUFFER_UAV_FLAG_RAW ? "raw r/w buffer" : desc.Buffer.StructureByteStride > 0 ? "structured r/w buffer" : "r/w buffer";
	case D3D_SRV_DIMENSION_TEXTURE1D:
		return "r/w texture1d";
	case D3D_SRV_DIMENSION_TEXTURE1DARRAY:
		return "r/w texture1d array";
	case D3D_SRV_DIMENSION_TEXTURE2D:
		return "r/w texture2d";
	case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
		return "r/w texture2d array";
	case D3D_SRV_DIMENSION_TEXTURE2DMS:
		return "r/w texture2d ms";
	case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
		return "r/w texture2d ms array";
	case D3D_SRV_DIMENSION_TEXTURE3D:
		return "r/w texture3d";
	case D3D_SRV_DIMENSION_TEXTURECUBE:
		return "r/w texturecube";
	case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
		return "r/w texturecube array";
	case D3D_SRV_DIMENSION_BUFFEREX:
		return desc.Buffer.Flags == D3D12_BUFFER_UAV_FLAG_RAW ? "raw r/w buffer ex" : desc.Buffer.StructureByteStride > 0 ? "structured r/w buffer ex" : "r/w buffer ex";
	default:
		return "r/w unmapped dimension";
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ASSIGN_TARGET(member) \
  member.m_pStateFlags = &(pParent->m_StateFlags);

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11ShaderStageState::Init(SCryDX11PipelineState* pParent)
{
	ASSIGN_TARGET(Shader);
	ASSIGN_TARGET(ConstantBufferViews);
	ASSIGN_TARGET(ConstBufferBindRange);
	ASSIGN_TARGET(ShaderResourceViews);
	ASSIGN_TARGET(UnorderedAccessViews);
	ASSIGN_TARGET(SamplerState);
}

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11ShaderStageState::DebugPrint()
{
	switch (Type)
	{
	case NCryDX12::ESS_Vertex:
		DX12_LOG(g_nPrintDX12, "Vertex shader stage:");
		break;
	case NCryDX12::ESS_Hull:
		DX12_LOG(g_nPrintDX12, "Hull shader stage:");
		break;
	case NCryDX12::ESS_Domain:
		DX12_LOG(g_nPrintDX12, "Domain shader stage:");
		break;
	case NCryDX12::ESS_Geometry:
		DX12_LOG(g_nPrintDX12, "Geometry shader stage:");
		break;
	case NCryDX12::ESS_Pixel:
		DX12_LOG(g_nPrintDX12, "Pixel shader stage:");
		break;
	case NCryDX12::ESS_Compute:
		DX12_LOG(g_nPrintDX12, "Compute shader stage:");
		break;
	}

	DX12_LOG(g_nPrintDX12, "Shader = %p %s",
	         Shader.Get(),
	         Shader.Get()->GetName().c_str()
	         );

	const NCryDX12::SResourceRanges& rr = Shader.Get()->GetDX12Shader()->GetResourceRanges();
	if (rr.m_ConstantBuffers.m_TotalLength +
	    rr.m_InputResources.m_TotalLength +
	    rr.m_OutputResources.m_TotalLength +
	    rr.m_Samplers.m_TotalLength)
	{
		DX12_LOG(g_nPrintDX12, " Resource Binding Table:");
	}

	if (rr.m_ConstantBuffers.m_TotalLength)
	{
		const NCryDX12::SBindRanges::TRanges& ranges = rr.m_ConstantBuffers.m_Ranges;
		for (size_t ri = 0, S = ranges.size(); ri < S; ++ri)
		{
			DX12_LOG(g_nPrintDX12, " C [%2d to %2d]:", ranges[ri].m_Start, ranges[ri].m_Start + ranges[ri].m_Length);
			for (size_t i = ranges[ri].m_Start; i < ranges[ri].m_Start + ranges[ri].m_Length; ++i)
			{
				if (ConstantBufferViews.Get(i))
				{
					DX12_LOG(g_nPrintDX12, "  %2zd: %p %p %lld+%d[%d] %s", i,
					         ConstantBufferViews.Get(i).get(),
					         &ConstantBufferViews.Get(i)->GetDX12Resource(),
					         ConstantBufferViews.Get(i)->GetDX12View().GetCBVDesc().BufferLocation,
					         ConstBufferBindRange.Get(i).start,
					         ConstBufferBindRange.Get(i).end - ConstBufferBindRange.Get(i).start,
					         ConstantBufferViews.Get(i)->GetName().c_str()
					         );
				}
				else
				{
					DX12_LOG(g_nPrintDX12, "  %2zd: ERROR! Null resource.", i);
				}
			}
		}
	}

	if (rr.m_InputResources.m_TotalLength)
	{
		const NCryDX12::SBindRanges::TRanges& ranges = rr.m_InputResources.m_Ranges;
		for (size_t ri = 0, S = ranges.size(); ri < S; ++ri)
		{
			DX12_LOG(g_nPrintDX12, " T [%2d to %2d]:", ranges[ri].m_Start, ranges[ri].m_Start + ranges[ri].m_Length);
			for (size_t i = ranges[ri].m_Start; i < ranges[ri].m_Start + ranges[ri].m_Length; ++i)
			{
				if (ShaderResourceViews.Get(i))
				{
					DX12_LOG(g_nPrintDX12, "  %2zd: %p %p [%s, %s, %s] %s", i,
					         ShaderResourceViews.Get(i).get(),
					         ShaderResourceViews.Get(i)->GetD3D12Resource(),
					         TypeToString(ranges[ri].m_Types[i - ranges[ri].m_Start]),
					         DimensionToString(ranges[ri].m_Dimensions[i - ranges[ri].m_Start]),
					         DimensionToString(ShaderResourceViews.Get(i)->GetDX12View().GetSRVDesc()),
					         ShaderResourceViews.Get(i)->GetResourceName().c_str()
					         );
				}
				else
				{
					DX12_LOG(g_nPrintDX12, "  %2zd: ERROR! Null resource.", i);
				}
			}
		}
	}

	if (rr.m_OutputResources.m_TotalLength)
	{
		const NCryDX12::SBindRanges::TRanges& ranges = rr.m_OutputResources.m_Ranges;
		for (size_t ri = 0, S = ranges.size(); ri < S; ++ri)
		{
			DX12_LOG(g_nPrintDX12, " U [%2d to %2d]:", ranges[ri].m_Start, ranges[ri].m_Start + ranges[ri].m_Length);
			for (size_t i = ranges[ri].m_Start; i < ranges[ri].m_Start + ranges[ri].m_Length; ++i)
			{
				if (UnorderedAccessViews.Get(i))
				{
					DX12_LOG(g_nPrintDX12, "  %2zd: %p %p [%s, %s, %s] %s", i,
					         UnorderedAccessViews.Get(i).get(),
					         UnorderedAccessViews.Get(i)->GetD3D12Resource(),
					         TypeToString(ranges[ri].m_Types[i - ranges[ri].m_Start]),
					         DimensionToString(ranges[ri].m_Dimensions[i - ranges[ri].m_Start]),
					         DimensionToString(UnorderedAccessViews.Get(i)->GetDX12View().GetUAVDesc()),
					         UnorderedAccessViews.Get(i)->GetResourceName().c_str()
					         );
				}
				else
				{
					DX12_LOG(g_nPrintDX12, "  %2zd: ERROR! Null resource.", i);
				}
			}
		}
	}

	if (rr.m_Samplers.m_TotalLength)
	{
		const NCryDX12::SBindRanges::TRanges& ranges = rr.m_Samplers.m_Ranges;
		for (size_t ri = 0, S = ranges.size(); ri < S; ++ri)
		{
			DX12_LOG(g_nPrintDX12, " S [%2d to %2d]", ranges[ri].m_Start, ranges[ri].m_Start + ranges[ri].m_Length);
			for (size_t i = ranges[ri].m_Start; i < ranges[ri].m_Start + ranges[ri].m_Length; ++i)
			{
				if (SamplerState.Get(i))
				{
					DX12_LOG(g_nPrintDX12, "  %2zd: %p", i,
					         SamplerState.Get(i).get()
					         );
				}
				else
				{
					DX12_LOG(g_nPrintDX12, "  %2zd: ERROR! Null resource.", i);
				}
			}
		}
	}

	DX12_LOG(g_nPrintDX12, "");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11IAState::Init(SCryDX11PipelineState* pParent)
{
	ASSIGN_TARGET(PrimitiveTopology);
	ASSIGN_TARGET(InputLayout);
	ASSIGN_TARGET(VertexBuffers);
	ASSIGN_TARGET(Strides);
	ASSIGN_TARGET(Offsets);
	ASSIGN_TARGET(NumVertexBuffers);
	ASSIGN_TARGET(IndexBuffer);
	ASSIGN_TARGET(IndexBufferFormat);
	ASSIGN_TARGET(IndexBufferOffset);
}

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11IAState::DebugPrint()
{
	DX12_LOG(g_nPrintDX12, "IA:");
	DX12_LOG(g_nPrintDX12, " PrimitiveTopology: %d => %s", PrimitiveTopology.Get(), TopologyToString(PrimitiveTopology.Get()));
	DX12_LOG(g_nPrintDX12, " InputLayout: %p", InputLayout.Get());

	if (NumVertexBuffers.Get())
		DX12_LOG(g_nPrintDX12, " VertexBuffers:");
	for (size_t i = 0; i < NumVertexBuffers.Get(); ++i)
	{
		if (VertexBuffers.Get(i))
		{
			DX12_LOG(g_nPrintDX12, "  %2zd: %p %p %s", i,
			         VertexBuffers.Get(i).get(),
			         VertexBuffers.Get(i)->GetD3D12Resource(),
			         VertexBuffers.Get(i)->GetName().c_str()
			         );
		}
	}

	DX12_LOG(g_nPrintDX12, " IndexBuffer:");
	DX12_LOG(g_nPrintDX12, "  --: %p %p %s",
	         IndexBuffer.Get(),
	         IndexBuffer.Get() ? IndexBuffer.Get()->GetD3D12Resource() : nullptr,
	         IndexBuffer.Get() ? IndexBuffer.Get()->GetName().c_str() : "-"
	         );

	DX12_LOG(g_nPrintDX12, "");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11RasterizerState::Init(SCryDX11PipelineState* pParent)
{
	ASSIGN_TARGET(DepthStencilState);
	ASSIGN_TARGET(RasterizerState);
	ASSIGN_TARGET(Viewports);
	ASSIGN_TARGET(NumViewports);
	ASSIGN_TARGET(Scissors);
	ASSIGN_TARGET(NumScissors);
	ASSIGN_TARGET(ScissorEnabled);
}

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11RasterizerState::DebugPrint()
{
	DX12_LOG(g_nPrintDX12, "Rasterizer state:");
	DX12_LOG(g_nPrintDX12, " DepthStencilState: %p", DepthStencilState.Get());
	DX12_LOG(g_nPrintDX12, " RasterizerState: %p", RasterizerState.Get());

	DX12_LOG(g_nPrintDX12, "");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11OutputMergerState::Init(SCryDX11PipelineState* pParent)
{
	ASSIGN_TARGET(BlendState);
	ASSIGN_TARGET(RenderTargetViews);
	ASSIGN_TARGET(DepthStencilView);
	ASSIGN_TARGET(SampleMask);
	ASSIGN_TARGET(NumRenderTargets);
	ASSIGN_TARGET(RTVFormats);
	ASSIGN_TARGET(DSVFormat);
	ASSIGN_TARGET(SampleDesc);
	ASSIGN_TARGET(StencilRef);
}

#undef ASSIGN_TARGET

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11OutputMergerState::DebugPrint()
{
	DX12_LOG(g_nPrintDX12, "Output merger:");
	DX12_LOG(g_nPrintDX12, " BlendState: %p", BlendState.Get());

	DX12_LOG(g_nPrintDX12, " DepthStencilView:");
	DX12_LOG(g_nPrintDX12, "  --: %p %p %s",
	         DepthStencilView.Get(),
	         DepthStencilView.Get() ? DepthStencilView.Get()->GetD3D12Resource() : nullptr,
	         DepthStencilView.Get() ? DepthStencilView.Get()->GetResourceName().c_str() : "-"
	         );

	if (NumRenderTargets.Get())
		DX12_LOG(g_nPrintDX12, " RenderTargetViews:");
	for (size_t i = 0; i < NumRenderTargets.Get(); ++i)
	{
		if (RenderTargetViews.Get(i))
		{
			DX12_LOG(g_nPrintDX12, "  %2zd: %p %p %s", i,
			         RenderTargetViews.Get(i).get(),
			         RenderTargetViews.Get(i)->GetD3D12Resource(),
			         RenderTargetViews.Get(i)->GetResourceName().c_str()
			         );
		}
	}

	DX12_LOG(g_nPrintDX12, "");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SCryDX11PipelineState::InitD3D12Descriptor(NCryDX12::CComputePSO::SInitParams& params, UINT nodeMask)
{
	auto& desc = params.m_Desc;
	ZeroMemory(&desc, sizeof(params.m_Desc));

	desc.NodeMask = nodeMask;

	desc.CS = Stages[NCryDX12::ESS_Compute].GetD3D12ShaderBytecode();

	params.m_CS = Stages[NCryDX12::ESS_Compute].Shader ? Stages[NCryDX12::ESS_Compute].Shader->GetDX12Shader() : NULL;
}

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11PipelineState::InitD3D12Descriptor(NCryDX12::CGraphicsPSO::SInitParams& params, UINT nodeMask)
{
	auto& desc = params.m_Desc;
	ZeroMemory(&desc, sizeof(params.m_Desc));

	desc.NodeMask = nodeMask;

	desc.VS = Stages[NCryDX12::ESS_Vertex].GetD3D12ShaderBytecode();
	desc.PS = Stages[NCryDX12::ESS_Pixel].GetD3D12ShaderBytecode();
	desc.HS = Stages[NCryDX12::ESS_Hull].GetD3D12ShaderBytecode();
	desc.DS = Stages[NCryDX12::ESS_Domain].GetD3D12ShaderBytecode();
	desc.GS = Stages[NCryDX12::ESS_Geometry].GetD3D12ShaderBytecode();

	params.m_VS = Stages[NCryDX12::ESS_Vertex].Shader ? Stages[NCryDX12::ESS_Vertex].Shader->GetDX12Shader() : NULL;
	params.m_HS = Stages[NCryDX12::ESS_Hull].Shader ? Stages[NCryDX12::ESS_Hull].Shader->GetDX12Shader() : NULL;
	params.m_DS = Stages[NCryDX12::ESS_Domain].Shader ? Stages[NCryDX12::ESS_Domain].Shader->GetDX12Shader() : NULL;
	params.m_GS = Stages[NCryDX12::ESS_Geometry].Shader ? Stages[NCryDX12::ESS_Geometry].Shader->GetDX12Shader() : NULL;
	params.m_PS = Stages[NCryDX12::ESS_Pixel].Shader ? Stages[NCryDX12::ESS_Pixel].Shader->GetDX12Shader() : NULL;

	// Set blend state
	if (OutputMerger.BlendState)
	{
		desc.BlendState = OutputMerger.BlendState->GetD3D12BlendDesc();
	}

	// Set sample mask
	desc.SampleMask = OutputMerger.SampleMask.Get();

	// Set rasterizer state
	if (Rasterizer.RasterizerState)
	{
		desc.RasterizerState = Rasterizer.RasterizerState->GetD3D12RasterizerDesc();
	}
	else
	{
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	}

	// Set depth/stencil state
	if (Rasterizer.DepthStencilState)
	{
		desc.DepthStencilState = Rasterizer.DepthStencilState->GetD3D12DepthStencilDesc();
	}

	// Apply input layout
	if (InputAssembler.InputLayout)
	{
		const CCryDX12InputLayout::TDescriptors& descriptors = InputAssembler.InputLayout->GetDescriptors();

		desc.InputLayout.pInputElementDescs = descriptors.empty() ? NULL : &(descriptors[0]);
		desc.InputLayout.NumElements = static_cast<UINT>(descriptors.size());
	}

	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	// Set primitive topology type
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	switch (InputAssembler.PrimitiveTopology.Get())
	{
	case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		break;
	case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		break;
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		break;
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		break;
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		break;
	case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		break;
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		break;
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		break;
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
		primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		break;
	default:
		if (InputAssembler.PrimitiveTopology.Get() >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
		{
			primTopo12 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		}
	}

	desc.PrimitiveTopologyType = primTopo12;

	// Set render targets
	desc.NumRenderTargets = OutputMerger.NumRenderTargets.Get();

	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		desc.RTVFormats[i] = i < desc.NumRenderTargets ? OutputMerger.RTVFormats.Get(i) : DXGI_FORMAT_UNKNOWN;
	}

	desc.DSVFormat = OutputMerger.DSVFormat.Get();

	// Set sample descriptor
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
}

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11PipelineState::InitRootSignatureInitParams(NCryDX12::CRootSignature::SInitParams& params)
{
	params.m_VS = Stages[NCryDX12::ESS_Vertex].Shader ? Stages[NCryDX12::ESS_Vertex].Shader->GetDX12Shader() : NULL;
	params.m_HS = Stages[NCryDX12::ESS_Hull].Shader ? Stages[NCryDX12::ESS_Hull].Shader->GetDX12Shader() : NULL;
	params.m_DS = Stages[NCryDX12::ESS_Domain].Shader ? Stages[NCryDX12::ESS_Domain].Shader->GetDX12Shader() : NULL;
	params.m_GS = Stages[NCryDX12::ESS_Geometry].Shader ? Stages[NCryDX12::ESS_Geometry].Shader->GetDX12Shader() : NULL;
	params.m_PS = Stages[NCryDX12::ESS_Pixel].Shader ? Stages[NCryDX12::ESS_Pixel].Shader->GetDX12Shader() : NULL;

	params.m_CS = Stages[NCryDX12::ESS_Compute].Shader ? Stages[NCryDX12::ESS_Compute].Shader->GetDX12Shader() : NULL;
}

extern int g_nPrintDX12;

//---------------------------------------------------------------------------------------------------------------------
void SCryDX11PipelineState::DebugPrint()
{
	if (!g_nPrintDX12)
		return;

	// General
	for (size_t i = 0; i < NCryDX12::ESS_Num; ++i)
	{
		if (Stages[i].Shader.Get())
		{
			Stages[i].DebugPrint();
		}
	}

	// Graphics Fixed Function
	InputAssembler.DebugPrint();
	Rasterizer.DebugPrint();
	OutputMerger.DebugPrint();
}
