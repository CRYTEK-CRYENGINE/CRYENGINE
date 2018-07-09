// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLDeviceContext.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11DeviceContext
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLBlendState.hpp"
#include "CCryDXGLBuffer.hpp"
#include "CCryDXGLDepthStencilView.hpp"
#include "CCryDXGLDeviceContext.hpp"
#include "CCryDXGLDevice.hpp"
#include "CCryDXGLBlendState.hpp"
#include "CCryDXGLDepthStencilState.hpp"
#include "CCryDXGLInputLayout.hpp"
#include "CCryDXGLQuery.hpp"
#include "CCryDXGLRasterizerState.hpp"
#include "CCryDXGLRenderTargetView.hpp"
#include "CCryDXGLSamplerState.hpp"
#include "CCryDXGLShader.hpp"
#include "CCryDXGLShaderResourceView.hpp"
#include "CCryDXGLUnorderedAccessView.hpp"
#include "../Implementation/GLDevice.hpp"
#include "../Implementation/GLFormat.hpp"
#include "../Implementation/GLView.hpp"

#define DXGL_CHECK_HAZARDS         0
#define DXGL_CHECK_PIPELINE        0
#define DXGL_CHECK_CURRENT_CONTEXT 0

////////////////////////////////////////////////////////////////////////////////
// CCryDXGLDeviceContext
////////////////////////////////////////////////////////////////////////////////

CCryDXGLDeviceContext::CCryDXGLDeviceContext()
	: m_pContext(nullptr)
	, m_uStencilRef(0)
	, m_uSampleMask(0xFFFFFFFF)
	, m_eIndexBufferFormat(DXGI_FORMAT_UNKNOWN)
	, m_uIndexBufferOffset(0)
	, m_ePrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
	, m_uNumViewports(0)
	, m_uNumScissorRects(0)
	, m_bPredicateValue(false)
{
	DXGL_INITIALIZE_INTERFACE(D3D11DeviceContext)

	m_auBlendFactor[0] = 1.0f;
	m_auBlendFactor[1] = 1.0f;
	m_auBlendFactor[2] = 1.0f;
	m_auBlendFactor[3] = 1.0f;

	for (uint32 uVB = 0; uVB < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++uVB)
	{
		m_auVertexBufferStrides[uVB] = 0;
		m_auVertexBufferOffsets[uVB] = 0;
	}

	for (uint32 uSO = 0; uSO < D3D11_SO_BUFFER_SLOT_COUNT; ++uSO)
	{
		m_auStreamOutputBufferOffsets[uSO] = 0;
	}

	m_kStages.resize(NCryOpenGL::eST_NUM);
	std::vector<SStage>::iterator kStageIter(m_kStages.begin());
	const std::vector<SStage>::iterator kStageEnd(m_kStages.end());
	while (kStageIter != kStageEnd)
	{
		memset(kStageIter->m_auConstantBufferOffsets, 0, sizeof(kStageIter->m_auConstantBufferOffsets));
		memset(kStageIter->m_auConstantBufferSizes, 0, sizeof(kStageIter->m_auConstantBufferSizes));
		++kStageIter;
	}
}

CCryDXGLDeviceContext::~CCryDXGLDeviceContext()
{
	Shutdown();
}

bool CCryDXGLDeviceContext::Initialize(CCryDXGLDevice* pDevice)
{
	SetDevice(pDevice);

	m_spDefaultBlendState = CreateDefaultBlendState(pDevice);
	m_spDefaultDepthStencilState = CreateDefaultDepthStencilState(pDevice);
	m_spDefaultRasterizerState = CreateDefaultRasterizerState(pDevice);
	m_spDefaultSamplerState = CreateDefaultSamplerState(pDevice);

	NCryOpenGL::CDevice* pGLDevice(pDevice->GetGLDevice());
	m_pContext = pGLDevice->AllocateContext();
	pGLDevice->BindContext(m_pContext);

	bool bResult =
	  m_spDefaultBlendState->Initialize(pDevice, m_pContext) &&
	  m_spDefaultDepthStencilState->Initialize(pDevice, m_pContext) &&
	  m_spDefaultRasterizerState->Initialize(pDevice, m_pContext) &&
	  m_spDefaultSamplerState->Initialize(m_pContext);

	pGLDevice->UnbindContext(m_pContext);

	return bResult;
}

void CCryDXGLDeviceContext::Shutdown()
{
	if (m_pContext != NULL)
	{
		m_pContext->GetDevice()->FreeContext(m_pContext);
		m_pContext = NULL;
	}
	m_pDevice = NULL;
}

NCryOpenGL::CContext* CCryDXGLDeviceContext::GetGLContext()
{
	return m_pContext;
}

_smart_ptr<CCryDXGLBlendState> CCryDXGLDeviceContext::CreateDefaultBlendState(CCryDXGLDevice* pDevice)
{
	// Default D3D11_BLEND_DESC values from DXSDK
	D3D11_BLEND_DESC kDesc;
	ZeroMemory(&kDesc, sizeof(kDesc));
	kDesc.AlphaToCoverageEnable = FALSE;
	kDesc.IndependentBlendEnable = FALSE;
	kDesc.RenderTarget[0].BlendEnable = FALSE;
	kDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	kDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	kDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	kDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	kDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	kDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	kDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	return new CCryDXGLBlendState(kDesc, pDevice);
}

_smart_ptr<CCryDXGLDepthStencilState> CCryDXGLDeviceContext::CreateDefaultDepthStencilState(CCryDXGLDevice* pDevice)
{
	// Default D3D11_DEPTH_STENCIL_DESC values from DXSDK
	D3D11_DEPTH_STENCIL_DESC kDesc;
	kDesc.DepthEnable = TRUE;
	kDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	kDesc.DepthFunc = D3D11_COMPARISON_LESS;
	kDesc.StencilEnable = FALSE;
	kDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	kDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	kDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	kDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	kDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	kDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	kDesc.BackFace = kDesc.FrontFace;

	return new CCryDXGLDepthStencilState(kDesc, pDevice);
}

_smart_ptr<CCryDXGLRasterizerState> CCryDXGLDeviceContext::CreateDefaultRasterizerState(CCryDXGLDevice* pDevice)
{
	// Default D3D11_RASTERIZER_DESC values from DXSDK
	D3D11_RASTERIZER_DESC kDesc;
	kDesc.FillMode = D3D11_FILL_SOLID;
	kDesc.CullMode = D3D11_CULL_BACK;
	kDesc.FrontCounterClockwise = FALSE;
	kDesc.DepthBias = 0;
	kDesc.SlopeScaledDepthBias = 0.0f;
	kDesc.DepthBiasClamp = 0.0f;
	kDesc.DepthClipEnable = TRUE;
	kDesc.ScissorEnable = FALSE;
	kDesc.MultisampleEnable = FALSE;
	kDesc.AntialiasedLineEnable = FALSE;

	return new CCryDXGLRasterizerState(kDesc, pDevice);
}

_smart_ptr<CCryDXGLSamplerState> CCryDXGLDeviceContext::CreateDefaultSamplerState(CCryDXGLDevice* pDevice)
{
	// Default D3D11_SAMPLER_DESC values from DXSDK
	D3D11_SAMPLER_DESC kDesc;
	kDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	kDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	kDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	kDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	kDesc.MinLOD = -FLT_MAX;
	kDesc.MaxLOD = FLT_MAX;
	kDesc.MipLODBias = 0.0f;
	kDesc.MaxAnisotropy = 1;
	kDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	kDesc.BorderColor[0] = 1.0f;
	kDesc.BorderColor[1] = 1.0f;
	kDesc.BorderColor[2] = 1.0f;
	kDesc.BorderColor[3] = 1.0f;

	return new CCryDXGLSamplerState(kDesc, pDevice);
}

#if DXGL_CHECK_HAZARDS
void CheckHazard(uint32 uRTVIndex, CCryDXGLRenderTargetView* pRTView, CCryDXGLResource* pRTVResource, uint32 uSRVIndex, CCryDXGLShaderResourceView* pSRView, CCryDXGLResource* pSRVResource, uint32 uStage)
{
	if (pRTVResource == pSRVResource)
	{
		DXGL_WARNING("Hazard detected: render target view %d and shader resource view %d in stage %d refer to the same resource", uRTVIndex, uSRVIndex, uStage);
	}
}

void CheckHazard(uint32 uDSVIndex, CCryDXGLDepthStencilView* pDSView, CCryDXGLResource* pDSVResource, uint32 uSRVIndex, CCryDXGLShaderResourceView* pSRView, CCryDXGLResource* pSRVResource, uint32 uStage)
{
	if (pDSVResource == pSRVResource)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC kDSVDesc;
		pDSView->GetDesc(&kDSVDesc);
		if ((kDSVDesc.Flags & D3D11_DSV_READ_ONLY_DEPTH) == 0 || (kDSVDesc.Flags & D3D11_DSV_READ_ONLY_STENCIL) == 0)
		{
			DXGL_ERROR("Hazard detected: writable depth stencil view and shader resource view %d in stage %d refer to the same resource", uSRVIndex, uStage);
		}
	}
}

template<typename OMView>
void CheckHazards(_smart_ptr<OMView>* pOMViews, uint32 uNumOMViews, _smart_ptr<CCryDXGLShaderResourceView>* pSRViews, uint32 uNumSRViews, uint32 uStage)
{
	uint32 uOMView;
	for (uOMView = 0; uOMView < uNumOMViews; ++uOMView)
	{
		OMView* pOMView(pOMViews[uOMView]);
		if (pOMView != NULL)
		{
			ID3D11Resource* pOMVResource(NULL);
			pOMView->GetResource(&pOMVResource);

			uint32 uSRView;
			for (uSRView = 0; uSRView < uNumSRViews; ++uSRView)
			{
				CCryDXGLShaderResourceView* pSRView(pSRViews[uSRView]);
				if (pSRView != NULL)
				{
					ID3D11Resource* pSRVResource(NULL);
					pSRView->GetResource(&pSRVResource);

					CheckHazard(uOMView, pOMView, pOMVResource, uSRView, pSRView, pSRVResource, uStage);

					pSRVResource->Release();
				}
			}

			pOMVResource->Release();
		}
	}
}

#endif //DXGL_CHECK_HAZARDS

#if DXGL_CHECK_PIPELINE
template<typename Stage>
void CheckRequiredStage(std::vector<Stage>& kStages, uint32 uRequiredStage)
{
	if (kStages.size() <= uRequiredStage || kStages.at(uRequiredStage).m_spShader == NULL)
	{
		DXGL_ERROR("Required pipeline stage %d is not bound to a valid shader");
	}
}
template<typename Stage>
void CheckPipeline(std::vector<Stage>& kStages)
{
	CheckRequiredStage(kStages, NCryOpenGL::eST_Vertex);
	CheckRequiredStage(kStages, NCryOpenGL::eST_Fragment);
}
#else
template<typename Stage>
ILINE void CheckPipeline(std::vector<Stage>& kStages) {}
#endif //DXGL_CHECK_PIPELINE

#if DXGL_FULL_EMULATION
void CheckCurrentContext(NCryOpenGL::CContext* pContext)
{
	if (pContext->GetDevice()->GetCurrentContext() != pContext)
	{
		pContext->GetDevice()->BindContext(pContext);
	}
}
#elif DXGL_CHECK_CURRENT_CONTEXT
void CheckCurrentContext(NCryOpenGL::CContext* pContext)
{
	if (pContext->GetDevice()->GetCurrentContext() != pContext)
	{
		DXGL_ERROR("Device context has not been bound to this thread");
	}
}
#else
ILINE void CheckCurrentContext(NCryOpenGL::CContext*) {}
#endif

////////////////////////////////////////////////////////////////////////////////
// ID3D11DeviceContext implementation
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLDeviceContext::SetShaderResources(uint32 uStage, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	CheckCurrentContext(m_pContext);

	if (uStage >= m_kStages.size())
	{
		for (uint32 uView = 0; uView < NumViews; ++uView)
		{
			if (ppShaderResourceViews[uView] != NULL)
			{
				DXGL_ERROR("CCryDXGLDeviceContext::SetShaderResources: shader stage is not supported, setting will be ignored");
				break;
			}
		}
		return;
	}

	SStage& kStage(m_kStages[uStage]);
	for (uint32 uView = 0; uView < NumViews; ++uView)
	{
		uint32 uSlot(StartSlot + uView);
		CCryDXGLShaderResourceView* pDXGLShaderResourceView(CCryDXGLShaderResourceView::FromInterface(ppShaderResourceViews[uView]));
		if (kStage.m_aspShaderResourceViews[uSlot] != pDXGLShaderResourceView)
		{
			kStage.m_aspShaderResourceViews[uSlot] = pDXGLShaderResourceView;
#if OGL_SINGLE_CONTEXT
			m_pContext->SetShaderResourceView(pDXGLShaderResourceView == NULL ? NULL : pDXGLShaderResourceView->GetGLView(m_pContext), uStage, uSlot);
#else
			m_pContext->SetShaderResourceView(pDXGLShaderResourceView == NULL ? NULL : pDXGLShaderResourceView->GetGLView(), uStage, uSlot);
#endif
		}
	}

#if DXGL_CHECK_HAZARDS
	CheckHazards(m_aspRenderTargetViews, DXGL_ARRAY_SIZE(m_aspRenderTargetViews), kStage.m_aspShaderResourceViews, DXGL_ARRAY_SIZE(kStage.m_aspShaderResourceViews), uStage);
	CheckHazards(&m_spDepthStencilView, 1, kStage.m_aspShaderResourceViews, DXGL_ARRAY_SIZE(kStage.m_aspShaderResourceViews), uStage);
#endif //DXGL_CHECK_HAZARDS
}

void CCryDXGLDeviceContext::SetUnorderedAccessViews(uint32 uStage, UINT StartSlot, UINT NumViews, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews)
{
	CheckCurrentContext(m_pContext);

	if (uStage >= m_kStages.size())
	{
		for (uint32 uView = 0; uView < NumViews; ++uView)
		{
			if (ppUnorderedAccessViews[uView] != NULL)
			{
				DXGL_ERROR("CCryDXGLDeviceContext::SetUnorderedAccessViews: shader stage is not supported, setting will be ignored");
				break;
			}
		}
		return;
	}

	SStage& kStage(m_kStages[uStage]);
	for (uint32 uView = 0; uView < NumViews; ++uView)
	{
		uint32 uSlot(StartSlot + uView);
		CCryDXGLUnorderedAccessView* pDXGLUnorderedAccessView(CCryDXGLUnorderedAccessView::FromInterface(ppUnorderedAccessViews[uView]));
		if (kStage.m_aspUnorderedAccessViews[uSlot] != pDXGLUnorderedAccessView)
		{
			kStage.m_aspUnorderedAccessViews[uSlot] = pDXGLUnorderedAccessView;
#if OGL_SINGLE_CONTEXT
			m_pContext->SetUnorderedAccessView(pDXGLUnorderedAccessView == NULL ? NULL : pDXGLUnorderedAccessView->GetGLView(m_pContext), uStage, uSlot);
#else
			m_pContext->SetUnorderedAccessView(pDXGLUnorderedAccessView == NULL ? NULL : pDXGLUnorderedAccessView->GetGLView(), uStage, uSlot);
#endif
		}
	}

#if DXGL_CHECK_HAZARDS
	CheckHazards(m_aspRenderTargetViews, DXGL_ARRAY_SIZE(m_aspRenderTargetViews), kStage.m_aspUnorderedAccessViews, DXGL_ARRAY_SIZE(kStage.m_aspUnorderedAccessViews), uStage);
	CheckHazards(&m_spDepthStencilView, 1, kStage.m_aspUnorderedAccessViews, DXGL_ARRAY_SIZE(kStage.m_aspUnorderedAccessViews), uStage);
#endif //DXGL_CHECK_HAZARDS
}

void CCryDXGLDeviceContext::SetShader(uint32 uStage, CCryDXGLShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
{
	if (ppClassInstances != NULL && NumClassInstances != 0)
		DXGL_WARNING("Class instances not supported");

	if (uStage >= m_kStages.size())
	{
		if (pShader != NULL)
			DXGL_ERROR("CCryDXGLDeviceContext::SetShader: shader stage is not supported, setting will be ignored");
		return;
	}

	if (m_kStages[uStage].m_spShader != pShader)
	{
		CheckCurrentContext(m_pContext);
		m_kStages[uStage].m_spShader = pShader;
		m_pContext->SetShader(pShader == NULL ? NULL : pShader->GetGLShader(), uStage);
	}
}

void CCryDXGLDeviceContext::SetSamplers(uint32 uStage, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)
{
	CheckCurrentContext(m_pContext);

	if (uStage >= m_kStages.size())
	{
		for (uint32 uSampler = 0; uSampler < NumSamplers; ++uSampler)
		{
			if (ppSamplers[uSampler] != NULL)
			{
				DXGL_ERROR("CCryDXGLDeviceContext::SetSamplers: shader stage is not supported, setting will be ignored");
				break;
			}
		}
		return;
	}

	SStage& kStage(m_kStages[uStage]);
	for (uint32 uSampler = 0; uSampler < NumSamplers; ++uSampler)
	{
		uint32 uSlot(uSampler + StartSlot);

		CCryDXGLSamplerState* pSamplerState(CCryDXGLSamplerState::FromInterface(ppSamplers[uSampler]));
		if (pSamplerState == NULL)
			pSamplerState = m_spDefaultSamplerState.get();

		if (pSamplerState != kStage.m_aspSamplerStates[uSlot])
		{
			kStage.m_aspSamplerStates[uSlot] = pSamplerState;
			pSamplerState->Apply(uStage, uSlot, m_pContext);
		}
	}
}

void CCryDXGLDeviceContext::SetConstantBuffers(uint32 uStage, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	CheckCurrentContext(m_pContext);

	if (uStage >= m_kStages.size())
	{
		for (uint32 uBuffer = 0; uBuffer < NumBuffers; ++uBuffer)
		{
			if (ppConstantBuffers[uBuffer] != NULL)
			{
				DXGL_ERROR("CCryDXGLDeviceContext::SetConstantBuffers: shader stage is not supported, setting will be ignored");
				break;
			}
		}
		return;
	}

	assert(uStage < m_kStages.size());
	SStage& kStage(m_kStages[uStage]);
	for (uint32 uBuffer = 0; uBuffer < NumBuffers; ++uBuffer)
	{
		uint32 uSlot(StartSlot + uBuffer);
		CCryDXGLBuffer* pConstantBuffer(CCryDXGLBuffer::FromInterface(ppConstantBuffers[uBuffer]));
		uint32 uOffset(pFirstConstant == NULL ? 0 : pFirstConstant[uBuffer] * 16);
		uint32 uSize(pNumConstants == NULL ? 0 : pNumConstants[uBuffer] * 16);
		if (kStage.m_aspConstantBuffers[uSlot] != pConstantBuffer ||
		    kStage.m_auConstantBufferOffsets[uSlot] != uOffset ||
		    kStage.m_auConstantBufferSizes[uSlot] != uSize)
		{
			kStage.m_aspConstantBuffers[uSlot] = pConstantBuffer;
			kStage.m_auConstantBufferOffsets[uSlot] = uOffset;
			kStage.m_auConstantBufferSizes[uSlot] = uSize;
			if (pConstantBuffer == NULL)
			{
				m_pContext->SetConstantBuffer(NULL, NCryOpenGL::SBufferRange(uOffset, uSize), uStage, uSlot);
			}
			else
			{
				NCryOpenGL::SBuffer* pGLBuffer(pConstantBuffer->GetGLBuffer());
				if (uSize == 0)
					uSize = pGLBuffer->m_uSize;
				m_pContext->SetConstantBuffer(pGLBuffer, NCryOpenGL::SBufferRange(uOffset, uSize), uStage, uSlot);
			}
		}
	}
}

void CCryDXGLDeviceContext::GetShaderResources(uint32 uStage, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
{
	if (uStage >= m_kStages.size())
	{
		DXGL_ERROR("CCryDXGLDeviceContext::GetShaderResources: shader stage is not supported, no entries returned");
		for (uint32 uView = 0; uView < NumViews; ++uView)
			ppShaderResourceViews[uView] = NULL;
		return;
	}

	SStage& kStage(m_kStages[uStage]);
	for (uint32 uView = 0; uView < NumViews; ++uView)
	{
		uint32 uSlot(StartSlot + uView);
		CCryDXGLShaderResourceView::ToInterface(ppShaderResourceViews + uView, kStage.m_aspShaderResourceViews[uSlot]);
		if (ppShaderResourceViews[uView] != NULL)
			ppShaderResourceViews[uView]->AddRef();
	}
}

void CCryDXGLDeviceContext::GetUnorderedAccesses(uint32 uStage, UINT StartSlot, UINT NumViews, ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
	if (uStage >= m_kStages.size())
	{
		DXGL_ERROR("CCryDXGLDeviceContext::GetUnorderedAccesses: shader stage is not supported, no entries returned");
		for (uint32 uView = 0; uView < NumViews; ++uView)
			ppUnorderedAccessViews[uView] = NULL;
		return;
	}

	SStage& kStage(m_kStages[uStage]);
	for (uint32 uView = 0; uView < NumViews; ++uView)
	{
		uint32 uSlot(StartSlot + uView);
		CCryDXGLUnorderedAccessView::ToInterface(ppUnorderedAccessViews + uView, kStage.m_aspUnorderedAccessViews[uSlot]);
		if (ppUnorderedAccessViews[uView] != NULL)
			ppUnorderedAccessViews[uView]->AddRef();
	}
}

void CCryDXGLDeviceContext::GetShader(uint32 uStage, CCryDXGLShader** ppShader, ID3D11ClassInstance** ppClassInstances, UINT* NumClassInstances)
{
	if (ppClassInstances != NULL)
		DXGL_WARNING("Class instances not supported");
	if (NumClassInstances != NULL)
		*NumClassInstances = 0;

	if (uStage >= m_kStages.size())
	{
		DXGL_ERROR("CCryDXGLDeviceContext::GetShader: shader stage is not supported, no shader returned");
		*ppShader = NULL;
		return;
	}

	*ppShader = m_kStages[uStage].m_spShader.get();
	if (*ppShader != NULL)
		(*ppShader)->AddRef();
}

void CCryDXGLDeviceContext::GetSamplers(uint32 uStage, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
{
	if (uStage >= m_kStages.size())
	{
		DXGL_ERROR("CCryDXGLDeviceContext::GetSamplers: shader stage is not supported, no entries returned");
		for (uint32 uSampler = 0; uSampler < NumSamplers; ++uSampler)
			ppSamplers[uSampler] = NULL;
		return;
	}

	SStage& kStage(m_kStages[uStage]);
	for (uint32 uSampler = 0; uSampler < NumSamplers; ++uSampler)
	{
		uint32 uSlot(uSampler + StartSlot);

		CCryDXGLSamplerState::ToInterface(ppSamplers + uSampler, kStage.m_aspSamplerStates[uSlot]);
		if (ppSamplers[uSampler] != NULL)
			ppSamplers[uSampler]->AddRef();
	}
}

void CCryDXGLDeviceContext::GetConstantBuffers(uint32 uStage, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
	if (uStage >= m_kStages.size())
	{
		DXGL_ERROR("CCryDXGLDeviceContext::GetConstantBuffers: shader stage is not supported, no entries returned");
		for (uint32 uBuffer = 0; uBuffer < NumBuffers; ++uBuffer)
		{
			ppConstantBuffers[uBuffer] = NULL;
			if (pFirstConstant != NULL)
				pFirstConstant[uBuffer] = 0;
			if (pNumConstants != NULL)
				pNumConstants[uBuffer] = 0;
		}
		return;
	}

	assert(uStage < m_kStages.size());
	SStage& kStage(m_kStages[uStage]);
	for (uint32 uBuffer = 0; uBuffer < NumBuffers; ++uBuffer)
	{
		uint32 uSlot(StartSlot + uBuffer);

		CCryDXGLBuffer::ToInterface(ppConstantBuffers + uBuffer, kStage.m_aspConstantBuffers[uSlot]);
		if (pFirstConstant != NULL)
			pFirstConstant[uBuffer] = kStage.m_auConstantBufferOffsets[uSlot];
		if (pNumConstants != NULL)
			pNumConstants[uBuffer] = kStage.m_auConstantBufferSizes[uSlot];
		if (ppConstantBuffers[uBuffer] != NULL)
			ppConstantBuffers[uBuffer]->AddRef();
	}
}

#define _IMPLEMENT_COMMON_SHADER_SETTERS(_Prefix, _ShaderInterface, _ShaderType, _Stage)                                                                                                        \
  void CCryDXGLDeviceContext::_Prefix ## SetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView * const* ppShaderResourceViews)                                             \
  {                                                                                                                                                                                             \
    SetShaderResources(_Stage, StartSlot, NumViews, ppShaderResourceViews);                                                                                                                     \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## SetShader(_ShaderInterface * pShader, ID3D11ClassInstance * const* ppClassInstances, UINT NumClassInstances)                                           \
  {                                                                                                                                                                                             \
    SetShader(_Stage, _ShaderType::FromInterface(pShader), ppClassInstances, NumClassInstances);                                                                                                \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## SetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState * const* ppSamplers)                                                                  \
  {                                                                                                                                                                                             \
    SetSamplers(_Stage, StartSlot, NumSamplers, ppSamplers);                                                                                                                                    \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## SetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer * const* ppConstantBuffers)                                                           \
  {                                                                                                                                                                                             \
    SetConstantBuffers(_Stage, StartSlot, NumBuffers, ppConstantBuffers);                                                                                                                       \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## SetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer * const* ppConstantBuffers, const UINT * pFirstConstant, const UINT * pNumConstants) \
  {                                                                                                                                                                                             \
    SetConstantBuffers(_Stage, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);                                                                                        \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## GetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView * *ppShaderResourceViews)                                                   \
  {                                                                                                                                                                                             \
    GetShaderResources(_Stage, StartSlot, NumViews, ppShaderResourceViews);                                                                                                                     \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## GetShader(_ShaderInterface * *ppShader, ID3D11ClassInstance * *ppClassInstances, UINT * NumClassInstances)                                             \
  {                                                                                                                                                                                             \
    CCryDXGLShader* pShader(NULL);                                                                                                                                                              \
    GetShader(_Stage, &pShader, ppClassInstances, NumClassInstances);                                                                                                                           \
    _ShaderType::ToInterface(ppShader, static_cast<_ShaderType*>(pShader));                                                                                                                     \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## GetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState * *ppSamplers)                                                                        \
  {                                                                                                                                                                                             \
    GetSamplers(_Stage, StartSlot, NumSamplers, ppSamplers);                                                                                                                                    \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## GetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer * *ppConstantBuffers)                                                                 \
  {                                                                                                                                                                                             \
    GetConstantBuffers(_Stage, StartSlot, NumBuffers, ppConstantBuffers);                                                                                                                       \
  }                                                                                                                                                                                             \
  void CCryDXGLDeviceContext::_Prefix ## GetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer * *ppConstantBuffers, UINT * pFirstConstant, UINT * pNumConstants)                   \
  {                                                                                                                                                                                             \
    GetConstantBuffers(_Stage, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);                                                                                        \
  }
_IMPLEMENT_COMMON_SHADER_SETTERS(VS, ID3D11VertexShader, CCryDXGLVertexShader, NCryOpenGL::eST_Vertex)
_IMPLEMENT_COMMON_SHADER_SETTERS(PS, ID3D11PixelShader, CCryDXGLPixelShader, NCryOpenGL::eST_Fragment)
#if DXGL_SUPPORT_GEOMETRY_SHADERS
_IMPLEMENT_COMMON_SHADER_SETTERS(GS, ID3D11GeometryShader, CCryDXGLGeometryShader, NCryOpenGL::eST_Geometry)
#else
_IMPLEMENT_COMMON_SHADER_SETTERS(GS, ID3D11GeometryShader, CCryDXGLGeometryShader, NCryOpenGL::eST_NUM)
#endif
#if DXGL_SUPPORT_TESSELLATION
_IMPLEMENT_COMMON_SHADER_SETTERS(HS, ID3D11HullShader, CCryDXGLHullShader, NCryOpenGL::eST_TessControl)
_IMPLEMENT_COMMON_SHADER_SETTERS(DS, ID3D11DomainShader, CCryDXGLDomainShader, NCryOpenGL::eST_TessEvaluation)
#else
_IMPLEMENT_COMMON_SHADER_SETTERS(HS, ID3D11HullShader, CCryDXGLHullShader, NCryOpenGL::eST_NUM)
_IMPLEMENT_COMMON_SHADER_SETTERS(DS, ID3D11DomainShader, CCryDXGLDomainShader, NCryOpenGL::eST_NUM)
#endif
#if DXGL_SUPPORT_COMPUTE
_IMPLEMENT_COMMON_SHADER_SETTERS(CS, ID3D11ComputeShader, CCryDXGLComputeShader, NCryOpenGL::eST_Compute)
#else
_IMPLEMENT_COMMON_SHADER_SETTERS(CS, ID3D11ComputeShader, CCryDXGLComputeShader, NCryOpenGL::eST_NUM)
#endif
#undef _IMPLEMENT_COMMON_SHADER_SETTERS

void CCryDXGLDeviceContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	CheckCurrentContext(m_pContext);
	CheckPipeline(m_kStages);
	m_pContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}

void CCryDXGLDeviceContext::Draw(UINT VertexCount, UINT StartVertexLocation)
{
	CheckCurrentContext(m_pContext);
	CheckPipeline(m_kStages);
	m_pContext->Draw(VertexCount, StartVertexLocation);
}

HRESULT CCryDXGLDeviceContext::Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	CheckCurrentContext(m_pContext);
	NCryOpenGL::SResource* pGLResource(CCryDXGLResource::FromInterface(pResource)->GetGLResource());
	if (pGLResource->m_pfMapSubresource)
	{
		return (*pGLResource->m_pfMapSubresource)(pGLResource, Subresource, MapType, MapFlags, pMappedResource, m_pContext) ? S_OK : E_FAIL;
	}
	else
	{
		DXGL_NOT_IMPLEMENTED
		return E_FAIL;
	}
}

void CCryDXGLDeviceContext::Unmap(ID3D11Resource* pResource, UINT Subresource)
{
	CheckCurrentContext(m_pContext);
	NCryOpenGL::SResource* pGLResource(CCryDXGLResource::FromInterface(pResource)->GetGLResource());
	if (pGLResource->m_pfUnmapSubresource)
	{
		(*pGLResource->m_pfUnmapSubresource)(pGLResource, Subresource, m_pContext);
	}
	else
	{
		DXGL_NOT_IMPLEMENTED
	}
}

void CCryDXGLDeviceContext::IASetInputLayout(ID3D11InputLayout* pInputLayout)
{
	CCryDXGLInputLayout* pDXGLInputLayout(CCryDXGLInputLayout::FromInterface(pInputLayout));
	if (m_spInputLayout != pDXGLInputLayout)
	{
		CheckCurrentContext(m_pContext);
		m_pContext->SetInputLayout(pDXGLInputLayout == NULL ? NULL : pDXGLInputLayout->GetGLLayout());
		m_spInputLayout = pDXGLInputLayout;
	}
}

void CCryDXGLDeviceContext::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets)
{
	CheckCurrentContext(m_pContext);
	uint32 uSlot;
	for (uSlot = 0; uSlot < NumBuffers; ++uSlot)
	{
		uint32 uSlotIndex(StartSlot + uSlot);
		CCryDXGLBuffer* pDXGLVertexBuffer(CCryDXGLBuffer::FromInterface(ppVertexBuffers[uSlot]));
		if (m_aspVertexBuffers[uSlotIndex] != pDXGLVertexBuffer ||
		    m_auVertexBufferStrides[uSlotIndex] != pStrides[uSlot] ||
		    m_auVertexBufferOffsets[uSlotIndex] != pOffsets[uSlot])
		{
			m_aspVertexBuffers[uSlotIndex] = pDXGLVertexBuffer;
			m_auVertexBufferStrides[uSlotIndex] = pStrides[uSlot];
			m_auVertexBufferOffsets[uSlotIndex] = pOffsets[uSlot];
			m_pContext->SetVertexBuffer(uSlotIndex, pDXGLVertexBuffer == NULL ? 0 : pDXGLVertexBuffer->GetGLBuffer(), pStrides[uSlot], pOffsets[uSlot]);
		}
	}
}

void CCryDXGLDeviceContext::IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset)
{
	CCryDXGLBuffer* pDXGLIndexBuffer(CCryDXGLBuffer::FromInterface(pIndexBuffer));

	m_spIndexBuffer = pDXGLIndexBuffer;
	m_eIndexBufferFormat = Format;
	m_uIndexBufferOffset = Offset;

	CheckCurrentContext(m_pContext);
	if (pDXGLIndexBuffer == NULL)
		m_pContext->SetIndexBuffer(NULL, GL_NONE, 0, 0);
	else
	{
		const NCryOpenGL::SGIFormatInfo* pFormatInfo;
		NCryOpenGL::EGIFormat eGIFormat(NCryOpenGL::GetGIFormat(Format));
		if (eGIFormat == NCryOpenGL::eGIF_NUM ||
		    (pFormatInfo = NCryOpenGL::GetGIFormatInfo(eGIFormat)) == NULL ||
		    pFormatInfo->m_pTexture == NULL ||
		    pFormatInfo->m_pUncompressed == NULL)
		{
			DXGL_ERROR("Invalid format for index buffer");
			return;
		}
		m_pContext->SetIndexBuffer(pDXGLIndexBuffer->GetGLBuffer(), pFormatInfo->m_pTexture->m_eDataType, pFormatInfo->m_pUncompressed->GetPixelBytes(), Offset);
	}
}

void CCryDXGLDeviceContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	CheckCurrentContext(m_pContext);
	CheckPipeline(m_kStages);
	m_pContext->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void CCryDXGLDeviceContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	CheckCurrentContext(m_pContext);
	CheckPipeline(m_kStages);
	m_pContext->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void CCryDXGLDeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	CheckCurrentContext(m_pContext);
	m_ePrimitiveTopology = Topology;
	m_pContext->SetPrimitiveTopology(Topology);
}

void CCryDXGLDeviceContext::Begin(ID3D11Asynchronous* pAsync)
{
	CheckCurrentContext(m_pContext);
#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SQuery* pQuery(CCryDXGLQuery::FromInterface(pAsync)->GetGLQuery(m_pContext));
#else
	NCryOpenGL::SQuery* pQuery(CCryDXGLQuery::FromInterface(pAsync)->GetGLQuery());
#endif
	if (pQuery)
		pQuery->Begin();
}

void CCryDXGLDeviceContext::End(ID3D11Asynchronous* pAsync)
{
	CheckCurrentContext(m_pContext);
#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SQuery* pQuery(CCryDXGLQuery::FromInterface(pAsync)->GetGLQuery(m_pContext));
#else
	NCryOpenGL::SQuery* pQuery(CCryDXGLQuery::FromInterface(pAsync)->GetGLQuery());
#endif
	if (pQuery)
		pQuery->End();
}

HRESULT CCryDXGLDeviceContext::GetData(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags)
{
	CheckCurrentContext(m_pContext);
#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SQuery* pQuery(CCryDXGLQuery::FromInterface(pAsync)->GetGLQuery(m_pContext));
#else
	NCryOpenGL::SQuery* pQuery(CCryDXGLQuery::FromInterface(pAsync)->GetGLQuery());
#endif
	if (pQuery)
		return pQuery->GetData(pData, DataSize, (GetDataFlags & D3D11_ASYNC_GETDATA_DONOTFLUSH) == 0 ? true : false) ? S_OK : E_FAIL;
	return E_FAIL;
}

void CCryDXGLDeviceContext::SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue)
{
	if (pPredicate != NULL)
	{
		DXGL_NOT_IMPLEMENTED
	}
	m_spPredicate = CCryDXGLQuery::FromInterface(pPredicate);
	m_bPredicateValue = PredicateValue == TRUE;
}

void CCryDXGLDeviceContext::OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)
{
	OMSetRenderTargetsAndUnorderedAccessViews(NumViews, ppRenderTargetViews, pDepthStencilView, NumViews, 0, NULL, NULL);
}

void CCryDXGLDeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts)
{
	NCryOpenGL::SOutputMergerView* apGLRenderTargetViews[DXGL_ARRAY_SIZE(m_aspRenderTargetViews)];
	NCryOpenGL::SOutputMergerView* pGLDepthStencilView(NULL);

	for (uint32 uRTV = 0; uRTV < NumRTVs; ++uRTV)
	{
		CCryDXGLRenderTargetView* pDXGLRenderTargetView(CCryDXGLRenderTargetView::FromInterface(ppRenderTargetViews[uRTV]));
		m_aspRenderTargetViews[uRTV] = pDXGLRenderTargetView;
#if OGL_SINGLE_CONTEXT
		apGLRenderTargetViews[uRTV] = pDXGLRenderTargetView == NULL ? NULL : pDXGLRenderTargetView->GetGLView(m_pContext);
#else
		apGLRenderTargetViews[uRTV] = pDXGLRenderTargetView == NULL ? NULL : pDXGLRenderTargetView->GetGLView();
#endif
	}
	if (UAVStartSlot == NumRTVs)
	{
		SetUnorderedAccessViews(NCryOpenGL::eST_Fragment, 0, NumUAVs, ppUnorderedAccessViews);
	}
	else
	{
		DXGL_ERROR("CCryDXGLDeviceContext::OMSetRenderTargetsAndUnorderedAccessViews - UAVStartSlot is expected to be equal to NumRTVs");
		NumUAVs = 0;
	}

	for (uint32 uRTV = NumRTVs; uRTV < DXGL_ARRAY_SIZE(m_aspRenderTargetViews); ++uRTV)
	{
		m_aspRenderTargetViews[uRTV] = NULL;
		apGLRenderTargetViews[uRTV] = NULL;
	}

	CCryDXGLDepthStencilView* pDXGLDepthStencilView(CCryDXGLDepthStencilView::FromInterface(pDepthStencilView));
	m_spDepthStencilView = pDXGLDepthStencilView;
	if (pDXGLDepthStencilView != NULL)
#if OGL_SINGLE_CONTEXT
		pGLDepthStencilView = pDXGLDepthStencilView->GetGLView(m_pContext);
#else
		pGLDepthStencilView = pDXGLDepthStencilView->GetGLView();
#endif

#if DXGL_CHECK_HAZARDS
	for (uint32 uStage = 0; uStage < m_kStages.size(); ++uStage)
	{
		CheckHazards(m_aspRenderTargetViews, DXGL_ARRAY_SIZE(m_aspRenderTargetViews), m_kStages.at(uStage).m_aspShaderResourceViews, DXGL_ARRAY_SIZE(m_kStages.at(uStage).m_aspShaderResourceViews), uStage);
		CheckHazards(&m_spDepthStencilView, 1, m_kStages.at(uStage).m_aspShaderResourceViews, DXGL_ARRAY_SIZE(m_kStages.at(uStage).m_aspShaderResourceViews), uStage);
	}
#endif //DXGL_CHECK_HAZARDS

	CheckCurrentContext(m_pContext);

	m_pContext->SetRenderTargets(NumRTVs, apGLRenderTargetViews, pGLDepthStencilView);
}

void CCryDXGLDeviceContext::OMSetBlendState(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)
{
	CheckCurrentContext(m_pContext);

	CCryDXGLBlendState* pDXGLBlendState(
	  pBlendState == NULL ?
	  m_spDefaultBlendState.get() :
	  CCryDXGLBlendState::FromInterface(pBlendState));

	if (pDXGLBlendState != m_spBlendState)
	{
		m_spBlendState = pDXGLBlendState;
		pDXGLBlendState->Apply(m_pContext);
	}

	m_uSampleMask = SampleMask;
	if (BlendFactor == NULL)
	{
		m_auBlendFactor[0] = 1.0f;
		m_auBlendFactor[1] = 1.0f;
		m_auBlendFactor[2] = 1.0f;
		m_auBlendFactor[3] = 1.0f;
	}
	else
	{
		m_auBlendFactor[0] = BlendFactor[0];
		m_auBlendFactor[1] = BlendFactor[1];
		m_auBlendFactor[2] = BlendFactor[2];
		m_auBlendFactor[3] = BlendFactor[3];
	}

	m_pContext->SetBlendColor(m_auBlendFactor[0], m_auBlendFactor[1], m_auBlendFactor[2], m_auBlendFactor[3]);
	m_pContext->SetSampleMask(m_uSampleMask);
}

void CCryDXGLDeviceContext::OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef)
{
	CheckCurrentContext(m_pContext);

	CCryDXGLDepthStencilState* pDXGLDepthStencilState(
	  pDepthStencilState == NULL ?
	  m_spDefaultDepthStencilState.get() :
	  CCryDXGLDepthStencilState::FromInterface(pDepthStencilState));

	if (pDXGLDepthStencilState != m_spDepthStencilState || m_uStencilRef != StencilRef)
	{
		m_spDepthStencilState = pDXGLDepthStencilState;
		m_uStencilRef = StencilRef;
		pDXGLDepthStencilState->Apply(StencilRef, m_pContext);
	}
}

void CCryDXGLDeviceContext::SOSetTargets(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets)
{
	for (uint32 uBuffer = 0; uBuffer < D3D11_SO_BUFFER_SLOT_COUNT; ++uBuffer)
	{
		CCryDXGLBuffer* pDXGLSOBuffer(NULL);
		uint32 uOffset(0);
		if (uBuffer < NumBuffers)
		{
			pDXGLSOBuffer = CCryDXGLBuffer::FromInterface(ppSOTargets[uBuffer]);
			uOffset = pOffsets[uBuffer];
		}

		if (m_aspStreamOutputBuffers[uBuffer] != pDXGLSOBuffer ||
		    m_auStreamOutputBufferOffsets[uBuffer] != uOffset)
		{
			DXGL_NOT_IMPLEMENTED
			  m_aspStreamOutputBuffers[uBuffer] = pDXGLSOBuffer;
			m_auStreamOutputBufferOffsets[uBuffer] = uOffset;
		}
	}
}

void CCryDXGLDeviceContext::DrawAuto(void)
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::DrawIndexedInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::DrawInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
#if DXGL_SUPPORT_COMPUTE
	CheckCurrentContext(m_pContext);
	m_pContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
#else
	DXGL_ERROR("CCryDXGLDeviceContext::Dispatch is not supported in this configuration");
#endif
}

void CCryDXGLDeviceContext::DispatchIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::RSSetState(ID3D11RasterizerState* pRasterizerState)
{
	CCryDXGLRasterizerState* pDXGLRasterizerState(
	  pRasterizerState == NULL ?
	  m_spDefaultRasterizerState.get() :
	  CCryDXGLRasterizerState::FromInterface(pRasterizerState));

	if (pDXGLRasterizerState != m_spRasterizerState)
	{
		CheckCurrentContext(m_pContext);
		m_spRasterizerState = pDXGLRasterizerState;
		m_spRasterizerState->Apply(m_pContext);
	}
}

void CCryDXGLDeviceContext::RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT* pViewports)
{
	m_uNumViewports = NumViewports;
	NCryOpenGL::Memcpy(m_akViewports, pViewports, min(sizeof(m_akViewports), sizeof(pViewports[0]) * NumViewports));

	CheckCurrentContext(m_pContext);
	m_pContext->SetViewports(NumViewports, pViewports);
}

void CCryDXGLDeviceContext::RSSetScissorRects(UINT NumRects, const D3D11_RECT* pRects)
{
	m_uNumScissorRects = NumRects;
	if (NumRects > 0)
		NCryOpenGL::Memcpy(m_akScissorRects, pRects, min(sizeof(m_akScissorRects), sizeof(pRects[0]) * NumRects));

	CheckCurrentContext(m_pContext);
	m_pContext->SetScissorRects(NumRects, pRects);
}

void CCryDXGLDeviceContext::CopySubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox)
{
	CCryDXGLResource* pDXGLDstResource(CCryDXGLResource::FromInterface(pDstResource));
	CCryDXGLResource* pDXGLSrcResource(CCryDXGLResource::FromInterface(pSrcResource));

	CheckCurrentContext(m_pContext);
	D3D11_RESOURCE_DIMENSION kDstType, kSrcType;
	pDXGLDstResource->GetType(&kDstType);
	pDXGLSrcResource->GetType(&kSrcType);

	if (kDstType == kSrcType)
	{
		switch (kDstType)
		{
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			NCryOpenGL::CopySubTexture(
			  static_cast<NCryOpenGL::STexture*>(pDXGLDstResource->GetGLResource()), DstSubresource, DstX, DstY, DstZ,
			  static_cast<NCryOpenGL::STexture*>(pDXGLSrcResource->GetGLResource()), SrcSubresource, pSrcBox, m_pContext);
			break;
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			NCryOpenGL::CopySubBuffer(
			  static_cast<NCryOpenGL::SBuffer*>(pDXGLDstResource->GetGLResource()), DstSubresource, DstX, DstY, DstZ,
			  static_cast<NCryOpenGL::SBuffer*>(pDXGLSrcResource->GetGLResource()), SrcSubresource, pSrcBox, m_pContext);
			break;
		default:
			assert(false);
			break;
		}
	}
}

void CCryDXGLDeviceContext::CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)
{
	CCryDXGLResource* pDXGLDstResource(CCryDXGLResource::FromInterface(pDstResource));
	CCryDXGLResource* pDXGLSrcResource(CCryDXGLResource::FromInterface(pSrcResource));

	D3D11_RESOURCE_DIMENSION kDstType, kSrcType;
	pDXGLDstResource->GetType(&kDstType);
	pDXGLSrcResource->GetType(&kSrcType);

	if (kDstType == kSrcType)
	{
		CheckCurrentContext(m_pContext);
		switch (kDstType)
		{
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			NCryOpenGL::CopyTexture(
			  static_cast<NCryOpenGL::STexture*>(pDXGLDstResource->GetGLResource()),
			  static_cast<NCryOpenGL::STexture*>(pDXGLSrcResource->GetGLResource()),
			  m_pContext);
			break;
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			NCryOpenGL::CopyBuffer(
			  static_cast<NCryOpenGL::SBuffer*>(pDXGLDstResource->GetGLResource()),
			  static_cast<NCryOpenGL::SBuffer*>(pDXGLSrcResource->GetGLResource()),
			  m_pContext);
			break;
		default:
			assert(false);
			break;
		}
	}
	else
	{
		DXGL_ERROR("CopyResource failed - source and destination are resources of different type");
	}
}

void CCryDXGLDeviceContext::UpdateSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	CheckCurrentContext(m_pContext);
	NCryOpenGL::SResource* pGLResource(CCryDXGLResource::FromInterface(pDstResource)->GetGLResource());
	if (pGLResource->m_pfUpdateSubresource)
	{
		(*pGLResource->m_pfUpdateSubresource)(pGLResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, m_pContext);
	}
	else
	{
		DXGL_NOT_IMPLEMENTED
	}
}

void CCryDXGLDeviceContext::CopyStructureCount(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView)
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4])
{
	CheckCurrentContext(m_pContext);
	CCryDXGLRenderTargetView* pDXGLRenderTargetView(CCryDXGLRenderTargetView::FromInterface(pRenderTargetView));
#if OGL_SINGLE_CONTEXT
	m_pContext->ClearRenderTarget(pDXGLRenderTargetView == NULL ? NULL : pDXGLRenderTargetView->GetGLView(m_pContext), ColorRGBA);
#else
	m_pContext->ClearRenderTarget(pDXGLRenderTargetView == NULL ? NULL : pDXGLRenderTargetView->GetGLView(), ColorRGBA);
#endif
}

void CCryDXGLDeviceContext::ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4])
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4])
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	CheckCurrentContext(m_pContext);
	CCryDXGLDepthStencilView* pDXGLDepthStencilView(CCryDXGLDepthStencilView::FromInterface(pDepthStencilView));
#if OGL_SINGLE_CONTEXT
	m_pContext->ClearDepthStencil(pDXGLDepthStencilView == NULL ? NULL : pDXGLDepthStencilView->GetGLView(m_pContext), (ClearFlags & D3D11_CLEAR_DEPTH) != 0, (ClearFlags & D3D11_CLEAR_STENCIL) != 0, Depth, Stencil);
#else
	m_pContext->ClearDepthStencil(pDXGLDepthStencilView == NULL ? NULL : pDXGLDepthStencilView->GetGLView(), (ClearFlags & D3D11_CLEAR_DEPTH) != 0, (ClearFlags & D3D11_CLEAR_STENCIL) != 0, Depth, Stencil);
#endif
}

void CCryDXGLDeviceContext::GenerateMips(ID3D11ShaderResourceView* pShaderResourceView)
{
	CheckCurrentContext(m_pContext);
#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SShaderView* pGLView(CCryDXGLShaderResourceView::FromInterface(pShaderResourceView)->GetGLView(m_pContext));
#else
	NCryOpenGL::SShaderView* pGLView(CCryDXGLShaderResourceView::FromInterface(pShaderResourceView)->GetGLView());
#endif
	switch (pGLView->m_eType)
	{
	case NCryOpenGL::SShaderView::eSVT_Texture:
		static_cast<NCryOpenGL::SShaderTextureBasedView*>(pGLView)->GenerateMipmaps(m_pContext);
		break;
	default:
		DXGL_ERROR("Generation of mipmaps for this type of resource view is not supported");
		break;
	}
}

void CCryDXGLDeviceContext::SetResourceMinLOD(ID3D11Resource* pResource, FLOAT MinLOD)
{
	DXGL_NOT_IMPLEMENTED
}

FLOAT CCryDXGLDeviceContext::GetResourceMinLOD(ID3D11Resource* pResource)
{
	DXGL_NOT_IMPLEMENTED
	return 0.0f;
}

void CCryDXGLDeviceContext::ResolveSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::ExecuteCommandList(ID3D11CommandList* pCommandList, BOOL RestoreContextState)
{
	DXGL_NOT_IMPLEMENTED
}

void CCryDXGLDeviceContext::CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts)
{
#if DXGL_SUPPORT_COMPUTE
	SetUnorderedAccessViews(NCryOpenGL::eST_Compute, StartSlot, NumUAVs, ppUnorderedAccessViews);
#else
	DXGL_ERROR("CCryDXGLDeviceContext::CSSetUnorderedAccessViews is not supported in this configuration");
#endif //DXGL_SUPPORT_COMPUTE
}

void CCryDXGLDeviceContext::IAGetInputLayout(ID3D11InputLayout** ppInputLayout)
{
	CCryDXGLInputLayout::ToInterface(ppInputLayout, m_spInputLayout);
	if ((*ppInputLayout) != NULL)
		(*ppInputLayout)->AddRef();
}

void CCryDXGLDeviceContext::IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets)
{
	uint32 uSlot;
	for (uSlot = 0; uSlot < NumBuffers; ++uSlot)
	{
		uint32 uSlotIndex(StartSlot + uSlot);

		CCryDXGLBuffer::ToInterface(ppVertexBuffers + uSlot, m_aspVertexBuffers[uSlotIndex]);
		if (ppVertexBuffers[uSlot] != NULL)
			ppVertexBuffers[uSlot]->AddRef();
		pStrides[uSlot] = m_auVertexBufferStrides[uSlotIndex];
		pOffsets[uSlot] = m_auVertexBufferOffsets[uSlotIndex];
	}
}

void CCryDXGLDeviceContext::IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset)
{
	CCryDXGLBuffer::ToInterface(pIndexBuffer, m_spIndexBuffer);
	if ((*pIndexBuffer) != NULL)
		(*pIndexBuffer)->AddRef();
	*Format = m_eIndexBufferFormat;
	*Offset = m_uIndexBufferOffset;
}

void CCryDXGLDeviceContext::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology)
{
	*pTopology = m_ePrimitiveTopology;
}

void CCryDXGLDeviceContext::GetPredication(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue)
{
	ID3D11Query* pQuery;
	CCryDXGLQuery::ToInterface(&pQuery, m_spPredicate);
	*ppPredicate = static_cast<ID3D11Predicate*>(pQuery);
	*pPredicateValue = (m_bPredicateValue ? TRUE : FALSE);
}

void CCryDXGLDeviceContext::OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView)
{
	OMGetRenderTargetsAndUnorderedAccessViews(NumViews, ppRenderTargetViews, ppDepthStencilView, 0, 0, NULL);
}

void CCryDXGLDeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
	for (uint32 uRTV = 0; uRTV < NumRTVs; ++uRTV)
	{
		CCryDXGLRenderTargetView::ToInterface(ppRenderTargetViews + uRTV, m_aspRenderTargetViews[uRTV]);
		if (ppRenderTargetViews[uRTV] != NULL)
			ppRenderTargetViews[uRTV]->AddRef();
	}
	CCryDXGLDepthStencilView::ToInterface(ppDepthStencilView, m_spDepthStencilView);
	if ((*ppDepthStencilView) != NULL)
		(*ppDepthStencilView)->AddRef();

	GetUnorderedAccesses(NCryOpenGL::eST_Fragment, 0, NumUAVs, ppUnorderedAccessViews);
}

void CCryDXGLDeviceContext::OMGetBlendState(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask)
{
	CCryDXGLBlendState::ToInterface(ppBlendState, m_spBlendState);
	if ((*ppBlendState) != NULL)
		(*ppBlendState)->AddRef();
	BlendFactor[0] = m_auBlendFactor[0];
	BlendFactor[1] = m_auBlendFactor[1];
	BlendFactor[2] = m_auBlendFactor[2];
	BlendFactor[3] = m_auBlendFactor[3];
	*pSampleMask = m_uSampleMask;
}

void CCryDXGLDeviceContext::OMGetDepthStencilState(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef)
{
	CCryDXGLDepthStencilState::ToInterface(ppDepthStencilState, m_spDepthStencilState);
	if ((*ppDepthStencilState) != NULL)
		(*ppDepthStencilState)->AddRef();
	*pStencilRef = m_uStencilRef;
}

void CCryDXGLDeviceContext::SOGetTargets(UINT NumBuffers, ID3D11Buffer** ppSOTargets)
{
	for (uint32 uBuffer = 0; uBuffer < NumBuffers; ++uBuffer)
	{
		CCryDXGLBuffer::ToInterface(ppSOTargets + uBuffer, m_aspStreamOutputBuffers[uBuffer]);
		if (ppSOTargets[uBuffer] != NULL)
			ppSOTargets[uBuffer]->AddRef();
	}
}

void CCryDXGLDeviceContext::RSGetState(ID3D11RasterizerState** ppRasterizerState)
{
	CCryDXGLRasterizerState::ToInterface(ppRasterizerState, m_spRasterizerState);
	if ((*ppRasterizerState) != NULL)
		(*ppRasterizerState)->AddRef();
}

void CCryDXGLDeviceContext::RSGetViewports(UINT* pNumViewports, D3D11_VIEWPORT* pViewports)
{
	if (pViewports != NULL)
		NCryOpenGL::Memcpy(pViewports, m_akViewports, min(sizeof(m_akViewports), sizeof(pViewports[0]) * *pNumViewports));
	*pNumViewports = m_uNumViewports;
}

void CCryDXGLDeviceContext::RSGetScissorRects(UINT* pNumRects, D3D11_RECT* pRects)
{
	if (pRects != NULL)
		NCryOpenGL::Memcpy(pRects, m_akScissorRects, min(sizeof(m_akScissorRects), sizeof(pRects[0]) * *pNumRects));
	*pNumRects = m_uNumScissorRects;
}

void CCryDXGLDeviceContext::CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
#if DXGL_SUPPORT_COMPUTE
	GetUnorderedAccesses(NCryOpenGL::eST_Compute, StartSlot, NumUAVs, ppUnorderedAccessViews);
#else
	DXGL_ERROR("CCryDXGLDeviceContext::CSGetUnorderedAccessViews is not supported in this configuration");
#endif //DXGL_SUPPORT_COMPUTE
}

void CCryDXGLDeviceContext::ClearState(void)
{
	CheckCurrentContext(m_pContext);

	// Common shader state
	for (uint32 uStage = 0; uStage < m_kStages.size(); ++uStage)
	{
		SStage& kStage(m_kStages.at(uStage));

		for (uint32 uSRV = 0; uSRV < DXGL_ARRAY_SIZE(kStage.m_aspShaderResourceViews); ++uSRV)
		{
			if (kStage.m_aspShaderResourceViews[uSRV] != NULL)
			{
				m_pContext->SetShaderTexture(NULL, uStage, uSRV);
				kStage.m_aspShaderResourceViews[uSRV] = NULL;
			}
		}

#if DXGL_SUPPORT_SHADER_IMAGES
		for (uint32 uUAV = 0; uUAV < DXGL_ARRAY_SIZE(kStage.m_aspUnorderedAccessViews); ++uUAV)
		{
			if (kStage.m_aspUnorderedAccessViews[uUAV] != NULL)
			{
				m_pContext->SetShaderImage(NULL, uStage, uUAV);
				kStage.m_aspUnorderedAccessViews[uUAV] = NULL;
			}
		}
#endif //DXGL_SUPPORT_SHADER_IMAGES

		for (uint32 uSampler = 0; uSampler < DXGL_ARRAY_SIZE(kStage.m_aspSamplerStates); ++uSampler)
		{
			if (kStage.m_aspSamplerStates[uSampler] != m_spDefaultSamplerState)
			{
				m_spDefaultSamplerState->Apply(uStage, uSampler, m_pContext);
				kStage.m_aspSamplerStates[uSampler] = m_spDefaultSamplerState;
			}
		}

		for (uint32 uCB = 0; uCB < DXGL_ARRAY_SIZE(kStage.m_aspConstantBuffers); ++uCB)
		{
			if (kStage.m_aspConstantBuffers[uCB] != NULL)
			{
				m_pContext->SetConstantBuffer(NULL, NCryOpenGL::SBufferRange(0, 0), uStage, uCB);
				kStage.m_aspConstantBuffers[uCB] = NULL;
				kStage.m_auConstantBufferOffsets[uCB] = 0;
				kStage.m_auConstantBufferSizes[uCB] = 0;
			}
		}

		if (m_kStages[uStage].m_spShader != NULL)
		{
			m_pContext->SetShader(NULL, uStage);
			m_kStages[uStage].m_spShader = NULL;
		}
	}

	// Vertex buffers
	for (uint32 uVB = 0; uVB < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++uVB)
	{
		if (m_aspVertexBuffers[uVB] != NULL ||
		    m_auVertexBufferStrides[uVB] != 0 ||
		    m_auVertexBufferOffsets[uVB] != 0)
		{
			m_pContext->SetVertexBuffer(uVB, NULL, 0, 0);
			m_aspVertexBuffers[uVB] = NULL;
			m_auVertexBufferStrides[uVB] = 0;
			m_auVertexBufferOffsets[uVB] = 0;
		}
	}

	// Index buffer
	if (m_spIndexBuffer != NULL ||
	    m_eIndexBufferFormat != DXGI_FORMAT_UNKNOWN ||
	    m_uIndexBufferOffset != 0)
	{
		m_pContext->SetIndexBuffer(NULL, GL_NONE, 0, 0);
		m_spIndexBuffer = NULL;
		m_eIndexBufferFormat = DXGI_FORMAT_UNKNOWN;
		m_uIndexBufferOffset = 0;
	}

	// Input layout
	if (m_spInputLayout != NULL)
	{
		m_pContext->SetInputLayout(NULL);
		m_spInputLayout = NULL;
	}

	// Primitive topology
	if (m_ePrimitiveTopology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
	{
		m_pContext->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
		m_ePrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}

	// Output merger state
	OMSetBlendState(NULL, NULL, 0xFFFFFFFF);
	OMSetDepthStencilState(NULL, 0);
	OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);

	// Rasterizer state
	m_uNumScissorRects = 0;
	m_pContext->SetScissorRects(0, NULL);
	m_uNumViewports;
	m_pContext->SetViewports(0, NULL);
	RSSetState(NULL);

	// Predication
	SetPredication(NULL, FALSE);

	// Stream output
	SOSetTargets(0, NULL, NULL);
}

void CCryDXGLDeviceContext::Flush(void)
{
	CheckCurrentContext(m_pContext);
	m_pContext->Flush();
}

D3D11_DEVICE_CONTEXT_TYPE CCryDXGLDeviceContext::GetType(void)
{
	DXGL_TODO("Modify when deferred contexts are supported")
	return D3D11_DEVICE_CONTEXT_IMMEDIATE;
}

UINT CCryDXGLDeviceContext::GetContextFlags(void)
{
	return 0;
}

HRESULT CCryDXGLDeviceContext::FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}
