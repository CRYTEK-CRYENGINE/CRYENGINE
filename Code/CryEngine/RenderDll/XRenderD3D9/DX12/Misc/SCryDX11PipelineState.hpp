// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:
//  Version:     v1.00
//  Created:     17/02/2015 by Jan Pinter
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef __CCRYDX12PSO__
	#define __CCRYDX12PSO__

	#include "DX12/Resource/Misc/CCryDX12InputLayout.hpp"
	#include "DX12/Resource/Misc/CCryDX12Shader.hpp"

	#include "DX12/Resource/State/CCryDX12BlendState.hpp"
	#include "DX12/Resource/State/CCryDX12DepthStencilState.hpp"
	#include "DX12/Resource/State/CCryDX12SamplerState.hpp"
	#include "DX12/Resource/State/CCryDX12RasterizerState.hpp"

	#include "DX12/Resource/View/CCryDX12DepthStencilView.hpp"
	#include "DX12/Resource/View/CCryDX12RenderTargetView.hpp"
	#include "DX12/Resource/View/CCryDX12ShaderResourceView.hpp"
	#include "DX12/Resource/View/CCryDX12UnorderedAccessView.hpp"

	#include "DX12/API/DX12PSO.hpp"

struct SCryDX11PipelineState;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum EPipelineStateProperty
{
	// Generic
	EPSP_PipelineState,
	EPSP_ConstantBuffers,
	EPSP_Resources,
	EPSP_Samplers,

	// Graphics Fixed Function
	EPSP_VertexBuffers,
	EPSP_IndexBuffer,
	EPSP_PrimitiveTopology,
	EPSP_Viewports,
	EPSP_RenderTargetViews,
	EPSP_DepthStencilView,
	EPSP_StencilRef,

	EPSP_Last,
};

enum EPipelineStatePropertyBits
{
	// Generic
	EPSPB_PipelineState   = BIT(EPSP_PipelineState),
	EPSPB_ConstantBuffers = BIT(EPSP_ConstantBuffers),
	EPSPB_Resources       = BIT(EPSP_Resources),
	EPSPB_Samplers        = BIT(EPSP_Samplers),

	// Graphics Fixed Function
	EPSPB_VertexBuffers       = BIT(EPSP_VertexBuffers),
	EPSPB_IndexBuffer         = BIT(EPSP_IndexBuffer),
	EPSPB_PrimitiveTopology   = BIT(EPSP_PrimitiveTopology),
	EPSPB_Viewports           = BIT(EPSP_Viewports),
	EPSPB_RenderTargetViews   = BIT(EPSP_RenderTargetViews),
	EPSPB_DepthStencilView    = BIT(EPSP_DepthStencilView),
	EPSPB_StencilRef          = BIT(EPSP_StencilRef),

	EPSPB_OutputResources     = EPSPB_RenderTargetViews | EPSPB_DepthStencilView,

	EPSPB_RenderTargetFormats = EPSPB_RenderTargetViews | EPSPB_PipelineState,
	EPSPB_DepthStencilFormat  = EPSPB_DepthStencilView | EPSPB_PipelineState,

	EPSPB_InputResources      = EPSPB_ConstantBuffers | EPSPB_Resources | EPSPB_Samplers,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T, EPipelineStatePropertyBits P>
struct SChangeTrackingValue
{
	UINT* m_pStateFlags;
	T     m_Value;

	SChangeTrackingValue(UINT* pStateFlags = NULL)
		: m_pStateFlags(pStateFlags)
	{

	}

	template<typename X>
	bool Set(const X& value)
	{
		if (m_Value != value)
		{
			m_Value = value;
			* m_pStateFlags |= (P);

			return true;
		}

		return false;
	}

	T& Get()
	{
		return m_Value;
	}
	const T& Get() const
	{
		return m_Value;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T, EPipelineStatePropertyBits P>
struct SChangeTrackingValue<DX12_PTR(T), P>
{
	UINT* m_pStateFlags;
	DX12_PTR(T) m_Value;

	SChangeTrackingValue(UINT* pStateFlags = NULL)
		: m_pStateFlags(pStateFlags)
	{

	}

	template<typename X>
	bool Set(const X& value)
	{
		if (m_Value != value)
		{
			m_Value = value;
			*m_pStateFlags |= (P);

			return true;
		}

		return false;
	}

	T* Get() const
	{
		return m_Value.get();
	}

	operator T*() const
	{
		return m_Value.get();
	}
	T* operator->() const
	{
		return m_Value.get();
	}
	T& operator*() const
	{
		return *(m_Value.get());
	}
	bool operator!() const
	{
		return m_Value.get() == nullptr;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T, size_t SIZE, EPipelineStatePropertyBits P>
struct SChangeTrackingArray
{
	UINT* m_pStateFlags;
	T     m_Array[SIZE];

	SChangeTrackingArray(UINT* pStateFlags = NULL)
		: m_pStateFlags(pStateFlags)
	{

	}

	template<typename X>
	bool Set(size_t index, const X& value)
	{
		if (m_Array[index] != value)
		{
			m_Array[index] = value;
			* m_pStateFlags |= (P);

			return true;
		}

		return false;
	}

	T& Get(size_t index)
	{
		return m_Array[index];
	}
	const T& Get(size_t index) const
	{
		return m_Array[index];
	}

	T* Ptr(size_t index)
	{
		return m_Array + index;
	}
	const T* Ptr(size_t index) const
	{
		return m_Array + index;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SCryDX11ShaderStageState
{
	NCryDX12::EShaderStage Type;

	SChangeTrackingValue<DX12_PTR(CCryDX12Shader), EPSPB_PipelineState> Shader;
	SChangeTrackingArray<DX12_PTR(CCryDX12Buffer), D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, EPSPB_ConstantBuffers>   ConstantBufferViews;
	SChangeTrackingArray<TRange<UINT>, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, EPSPB_ConstantBuffers>               ConstBufferBindRange;
	SChangeTrackingArray<DX12_PTR(CCryDX12ShaderResourceView), D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, EPSPB_Resources>  ShaderResourceViews;
	SChangeTrackingArray<DX12_PTR(CCryDX12UnorderedAccessView), D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, EPSPB_Resources> UnorderedAccessViews;
	SChangeTrackingArray<DX12_PTR(CCryDX12SamplerState), D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, EPSPB_Samplers>                SamplerState;

	void                         Init(SCryDX11PipelineState* pParent);

	const D3D12_SHADER_BYTECODE& GetD3D12ShaderBytecode() const;

	void                         DebugPrint();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SCryDX11IAState
{
	SChangeTrackingValue<D3D11_PRIMITIVE_TOPOLOGY, EPipelineStatePropertyBits(EPSPB_PrimitiveTopology | EPSPB_PipelineState)> PrimitiveTopology;

	SChangeTrackingValue<DX12_PTR(CCryDX12InputLayout), EPSPB_PipelineState>                                       InputLayout;
	SChangeTrackingArray<DX12_PTR(CCryDX12Buffer), D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, EPSPB_VertexBuffers> VertexBuffers;
	SChangeTrackingArray<UINT, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, EPSPB_VertexBuffers>                     Strides;
	SChangeTrackingArray<UINT, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, EPSPB_VertexBuffers>                     Offsets;
	SChangeTrackingValue<UINT, EPSPB_VertexBuffers>                   NumVertexBuffers;

	SChangeTrackingValue<DX12_PTR(CCryDX12Buffer), EPSPB_IndexBuffer> IndexBuffer;
	SChangeTrackingValue<DXGI_FORMAT, EPSPB_IndexBuffer>              IndexBufferFormat;
	SChangeTrackingValue<UINT, EPSPB_IndexBuffer>                     IndexBufferOffset;

	SCryDX11IAState()
	{
		PrimitiveTopology.m_Value = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		NumVertexBuffers.m_Value = 0;
	}

	void Init(SCryDX11PipelineState* pParent);

	void DebugPrint();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SCryDX11RasterizerState
{
	SChangeTrackingValue<DX12_PTR(CCryDX12DepthStencilState), EPSPB_PipelineState> DepthStencilState;
	SChangeTrackingValue<DX12_PTR(CCryDX12RasterizerState), EPSPB_PipelineState>   RasterizerState;

	SChangeTrackingArray<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, EPSPB_Viewports> Viewports;
	SChangeTrackingValue<UINT, EPSPB_Viewports> NumViewports;

	SChangeTrackingArray<D3D11_RECT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, EPSPB_Viewports> Scissors;
	SChangeTrackingValue<UINT, EPSPB_Viewports> NumScissors;
	SChangeTrackingValue<BOOL, EPSPB_Viewports> ScissorEnabled;

	SCryDX11RasterizerState()
	{
		NumViewports.m_Value = 0;
		NumScissors.m_Value = 0;
		ScissorEnabled.m_Value = 0;
	}

	void Init(SCryDX11PipelineState* pParent);

	void DebugPrint();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SCryDX11OutputMergerState
{
	SChangeTrackingValue<DX12_PTR(CCryDX12BlendState), EPSPB_PipelineState> BlendState;

	SChangeTrackingArray<DX12_PTR(CCryDX12RenderTargetView), D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, EPSPB_RenderTargetViews> RenderTargetViews;
	SChangeTrackingValue<UINT, EPSPB_RenderTargetViews> NumRenderTargets;
	SChangeTrackingArray<DXGI_FORMAT, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, EPSPB_RenderTargetFormats>                      RTVFormats;

	SChangeTrackingValue<DX12_PTR(CCryDX12DepthStencilView), EPSPB_DepthStencilView> DepthStencilView;
	SChangeTrackingValue<DXGI_FORMAT, EPSPB_DepthStencilFormat>                      DSVFormat;

	SChangeTrackingValue<UINT, EPSPB_PipelineState>             SampleMask;
	SChangeTrackingValue<DXGI_SAMPLE_DESC, EPSPB_PipelineState> SampleDesc;

	SChangeTrackingValue<UINT, EPSPB_StencilRef>                StencilRef;

	SCryDX11OutputMergerState()
	{
		SampleMask.m_Value = UINT_MAX;
		NumRenderTargets.m_Value = 0;
		DSVFormat.m_Value = DXGI_FORMAT_UNKNOWN;
		SampleDesc.m_Value.Count = 1;
		SampleDesc.m_Value.Quality = 0;
		StencilRef.m_Value = 0;
	}

	void Init(SCryDX11PipelineState* pParent);

	void DebugPrint();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SHashResource
{
	uint32 m_CB_PI;
	uint32 m_CB_PB;
	uint32 m_CB_PM;
	uint32 m_Samplers;
	uint32 m_Textures;

	SHashResource()
	{
		m_CB_PI = 0;
		m_CB_PB = 0;
		m_CB_PM = 0;
		m_Samplers = 0;
		m_Textures = 0;
	}
};

struct SCryDX11PipelineState
{
	UINT          m_StateFlags, m_StateFlagsEncountered;
	SHashResource m_HashRes;

	// General
	SCryDX11ShaderStageState Stages[NCryDX12::ESS_Num];

	// Graphics Fixed Function
	SCryDX11IAState           InputAssembler;
	SCryDX11RasterizerState   Rasterizer;
	SCryDX11OutputMergerState OutputMerger;

	SCryDX11PipelineState()
		: m_StateFlags(0)
		, m_StateFlagsEncountered(0)
	{
		// Generic
		for (size_t i = 0; i < NCryDX12::ESS_Num; ++i)
		{
			Stages[i].Type = static_cast<NCryDX12::EShaderStage>(i);
			Stages[i].Init(this);
		}

		// Graphics Fixed Function
		InputAssembler.Init(this);
		Rasterizer.Init(this);
		OutputMerger.Init(this);
	}

	void Invalidate()
	{
		m_StateFlags = m_StateFlagsEncountered;
	}

	bool AreShadersBound() const
	{
		return !(
		  Stages[NCryDX12::ESS_Vertex].Shader.m_Value == NULL &&
		  Stages[NCryDX12::ESS_Hull].Shader.m_Value == NULL &&
		  Stages[NCryDX12::ESS_Domain].Shader.m_Value == NULL &&
		  Stages[NCryDX12::ESS_Geometry].Shader.m_Value == NULL &&
		  Stages[NCryDX12::ESS_Pixel].Shader.m_Value == NULL &&
		  Stages[NCryDX12::ESS_Compute].Shader.m_Value == NULL
		  );
	}

	void InitD3D12Descriptor(NCryDX12::CGraphicsPSO::SInitParams& params, UINT nodeMask);
	void InitD3D12Descriptor(NCryDX12::CComputePSO::SInitParams& params, UINT nodeMask);
	void InitRootSignatureInitParams(NCryDX12::CRootSignature::SInitParams& params);

	void DebugPrint();
};

#endif
