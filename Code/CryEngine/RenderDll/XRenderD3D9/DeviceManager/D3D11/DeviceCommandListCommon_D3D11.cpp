// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../DeviceObjects.h"
#include "DriverD3D.h"
#include "../../../Common/ReverseDepth.h"

#include "DeviceCommandListCommon_D3D11.h"
#include "DeviceObjects_D3D11.h"
#include "DeviceResourceSet_D3D11.h"
#include "DevicePSO_D3D11.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(CRY_PLATFORM_ORBIS)

CDeviceTimestampGroup::CDeviceTimestampGroup()
	: m_numTimestamps(0)
	, m_pDisjointQuery(nullptr)
	, m_frequency(0L)
	, m_measurable(false)
{
	m_timestampQueries.fill(nullptr);
	m_timeValues.fill(0);
}

CDeviceTimestampGroup::~CDeviceTimestampGroup()
{
	SAFE_RELEASE(m_pDisjointQuery);
	for (uint32 i = 0; i < m_timestampQueries.size(); i++)
	{
		SAFE_RELEASE(m_timestampQueries[i]);
	}
}

void CDeviceTimestampGroup::Init()
{
	D3D11_QUERY_DESC queryTimestampDesc = { D3D11_QUERY_TIMESTAMP, 0 };
	D3D11_QUERY_DESC queryDisjointDesc = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };

	for (uint32 i = 0; i < m_timestampQueries.size(); i++)
	{
		gcpRendD3D->GetDevice().CreateQuery(&queryTimestampDesc, &m_timestampQueries[i]);
	}
	gcpRendD3D->GetDevice().CreateQuery(&queryDisjointDesc, &m_pDisjointQuery);
}

void CDeviceTimestampGroup::BeginMeasurement()
{
	m_numTimestamps = 0;
	m_frequency = 0;
	m_measurable = false;
	gcpRendD3D->GetDeviceContext().Begin(m_pDisjointQuery);
}

void CDeviceTimestampGroup::EndMeasurement()
{
	gcpRendD3D->GetDeviceContext().End(m_pDisjointQuery);
	m_measurable = true;
}

uint32 CDeviceTimestampGroup::IssueTimestamp(CDeviceCommandList* pCommandList)
{
	assert(m_numTimestamps < m_timestampQueries.size());
	gcpRendD3D->GetDeviceContext().End(m_timestampQueries[m_numTimestamps]);
	m_timeValues[m_numTimestamps] = 0;
	return m_numTimestamps++;
}

bool CDeviceTimestampGroup::ResolveTimestamps()
{
	if (!m_measurable)
		return false;

	// Don't ask twice (API violation)
	if (!m_frequency)
	{
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
		if (gcpRendD3D->GetDeviceContext().GetData(m_pDisjointQuery, &disjointData, m_pDisjointQuery->GetDataSize(), 0) == S_OK)
		{
			m_frequency = disjointData.Frequency;
		}
		else
		{
			m_frequency = 0;
			return false;
		}
	}

	int i = m_numTimestamps;
	while (--i >= 0)
	{
		// Don't ask twice (API violation)
		if (!m_timeValues[i])
		{
			if (gcpRendD3D->GetDeviceContext().GetData(m_timestampQueries[i], &m_timeValues[i], m_timestampQueries[i]->GetDataSize(), 0) != S_OK)
			{
				m_timeValues[i] = 0;
				return false;
			}
		}
	}

	return true;
}

#endif



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceNvidiaCommandInterfaceImpl* CDeviceCommandListImpl::GetNvidiaCommandInterfaceImpl()
{
	if (gcpRendD3D->m_bVendorLibInitialized && (gRenDev->GetFeatures() & RFT_HW_NVIDIA))
	{
		return reinterpret_cast<CDeviceNvidiaCommandInterfaceImpl*>(this);
	}

	return nullptr;
}

void CDeviceCommandListImpl::SetProfilerMarker(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)
	PROFILE_LABEL_GPU(label);
#endif
}

void CDeviceCommandListImpl::BeginProfilerEvent(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)
	PROFILE_LABEL_PUSH_GPU(label);
#endif
}

void CDeviceCommandListImpl::EndProfilerEvent(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)	
	PROFILE_LABEL_POP_GPU(label);
#endif
}

void CDeviceCommandListImpl::ClearStateImpl(bool bOutputMergerOnly) const
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	if (rd->GetDeviceContext().IsValid())
	{
		D3DDepthSurface* pDSV = 0;
		D3DSurface*      pRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { 0 };
		D3DUAV*          pUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT] = { 0 };

		if (bOutputMergerOnly)
			rd->GetDeviceContext().OMSetRenderTargets/*AndUnorderedAccessViews*/(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, pRTVs, pDSV/*, 0, D3D11_PS_CS_UAV_REGISTER_COUNT, pUAVs, nullptr*/);
		else
			rd->GetDeviceContext().ClearState();
	}

	rd->m_DevMan.ClearState();
}

void CDeviceCommandListImpl::ResetImpl()
{
	const uint8 InvalidShaderPointer = (uint8)(uint64)CDeviceResourceSet_DX11::InvalidPointer;
	CRY_ASSERT((InvalidShaderPointer & 0x1) != 0);

	memset(m_sharedState.shader, InvalidShaderPointer, sizeof(m_sharedState.shader));
	memset(m_sharedState.shaderResourceView, InvalidShaderPointer, sizeof(m_sharedState.shaderResourceView));
	memset(m_sharedState.samplerState, InvalidShaderPointer, sizeof(m_sharedState.samplerState));
	memset(m_sharedState.constantBuffer, InvalidShaderPointer, sizeof(m_sharedState.constantBuffer));
	m_sharedState.validShaderStages = EShaderStage_All;

	m_sharedState.numSRVs.fill(0);
	m_sharedState.numSamplers.fill(0);

	UINT_PTR invalidPtr = UINT_PTR(~0u);
	m_graphicsState.custom.depthStencilState.cachedValue = nullptr; 
	m_graphicsState.custom.rasterizerState.cachedValue = nullptr;
	m_graphicsState.custom.rasterizerStateIndex = 0;
	m_graphicsState.custom.blendState.cachedValue = nullptr;
	m_graphicsState.custom.inputLayout = reinterpret_cast<ID3D11InputLayout*>(invalidPtr);
	m_graphicsState.custom.topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_graphicsState.custom.bDepthStencilStateDirty = true;
	m_graphicsState.custom.depthConstBias = 0.0f;
	m_graphicsState.custom.depthSlopeBias = 0.0f;
	m_graphicsState.custom.depthBiasClamp = 0.0f;
	m_graphicsState.custom.bRasterizerStateDirty = true;

	m_computeState.custom.boundUAVs = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// *INDENT-OFF*
static_assert(
	(1 << eHWSC_Vertex  ) == EShaderStage_Vertex   &&
	(1 << eHWSC_Pixel   ) == EShaderStage_Pixel    &&
	(1 << eHWSC_Geometry) == EShaderStage_Geometry &&
	(1 << eHWSC_Domain  ) == EShaderStage_Domain   &&
	(1 << eHWSC_Hull    ) == EShaderStage_Hull,
	"EShaderStage should have bits corresponding with EHWShaderClass");

static_assert(
	eHWSC_Vertex   == CSubmissionQueue_DX11::TYPE_VS &&
	eHWSC_Pixel    == CSubmissionQueue_DX11::TYPE_PS &&
	eHWSC_Geometry == CSubmissionQueue_DX11::TYPE_GS &&
	eHWSC_Domain   == CSubmissionQueue_DX11::TYPE_DS &&
	eHWSC_Hull     == CSubmissionQueue_DX11::TYPE_HS &&
	eHWSC_Compute  == CSubmissionQueue_DX11::TYPE_CS,
	"SHADER_TYPE enumeration should match EHWShaderClass for performance reasons");

static inline void BindShader(EHWShaderClass shaderClass, ID3D11Resource* pBin)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->m_DevMan.BindShader(static_cast<CSubmissionQueue_DX11::SHADER_TYPE>(shaderClass), pBin);
}

static inline void BindGraphicsSRV(EHWShaderClass shaderClass, ID3D11ShaderResourceView* pSrv, uint32 slot)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->m_DevMan.BindSRV(static_cast<CSubmissionQueue_DX11::SHADER_TYPE>(shaderClass), pSrv, slot);
}

static inline void BindGraphicsSampler(EHWShaderClass shaderClass, ID3D11SamplerState* pSamplerState, uint32 slot)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->m_DevMan.BindSampler(static_cast<CSubmissionQueue_DX11::SHADER_TYPE>(shaderClass), pSamplerState, slot);
}

static inline void BindGraphicsConstantBuffer(EHWShaderClass shaderClass, D3DBuffer* pBuffer, uint32 slot, uint32 offset, uint32 size)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->m_DevMan.BindConstantBuffer(static_cast<CSubmissionQueue_DX11::SHADER_TYPE>(shaderClass), pBuffer, slot, offset, size);
}
// *INDENT-ON*

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandInterfaceImpl::BeginRenderPassImpl(const CDeviceRenderPass& renderPass, const D3DRectangle& renderArea)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().OMSetRenderTargets(renderPass.m_RenderTargetCount, renderPass.m_RenderTargetViews.data(), renderPass.m_pDepthStencilView);
}

void CDeviceGraphicsCommandInterfaceImpl::SetViewportsImpl(uint32 vpCount, const D3DViewPort* pViewports)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().RSSetViewports(vpCount, pViewports);
}

void CDeviceGraphicsCommandInterfaceImpl::SetScissorRectsImpl(uint32 rcCount, const D3DRectangle* pRects)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().RSSetScissorRects(rcCount, pRects);
}

void CDeviceGraphicsCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceGraphicsPSO* devicePSO)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const CDeviceGraphicsPSO_DX11* pDevicePSO = reinterpret_cast<const CDeviceGraphicsPSO_DX11*>(devicePSO);

	// RasterState, BlendState
	if (m_graphicsState.custom.blendState.Set(pDevicePSO->m_pBlendState.get()))
	{
		rd->m_DevMan.SetBlendState(pDevicePSO->m_pBlendState, NULL, 0xffffffff);
	}

	// Rasterizer state: NOTE - we don't know the final depth bias values yet so only mark dirty here and set in actual draw call
	if (m_graphicsState.custom.rasterizerState.Set(pDevicePSO->m_pRasterizerState.get()))
	{
		m_graphicsState.custom.rasterizerStateIndex = pDevicePSO->m_RasterizerStateIndex;
		m_graphicsState.custom.bRasterizerStateDirty = true;
	}

	// Depth stencil state: NOTE - we don't know the final stencil ref value yet so only mark dirty here and set in actual draw call
	if (m_graphicsState.custom.depthStencilState.Set(pDevicePSO->m_pDepthStencilState.get()))
	{
		m_graphicsState.custom.bDepthStencilStateDirty = true;
	}

	// input layout and topology
	if (m_graphicsState.custom.inputLayout.Set(pDevicePSO->m_pInputLayout.get()))   rd->m_DevMan.BindVtxDecl(pDevicePSO->m_pInputLayout);
	if (m_graphicsState.custom.topology.Set(pDevicePSO->m_PrimitiveTopology))       rd->m_DevMan.BindTopology(pDevicePSO->m_PrimitiveTopology);

	// Shaders and update valid shader mask
	m_sharedState.validShaderStages = EShaderStage_None;

	const std::array<void*, eHWSC_Num>& shaders = pDevicePSO->m_pDeviceShaders;
	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_NumGfx; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		m_sharedState.validShaderStages |= SHADERSTAGE_FROM_SHADERCLASS_CONDITIONAL(shaderClass, shaders[shaderClass] ? 1 : 0);

		if (m_sharedState.shader[shaderClass].Set(shaders[shaderClass]))
		{
			BindShader(shaderClass, (ID3D11Resource*)shaders[shaderClass]);
		}
	}

	m_sharedState.srvs = pDevicePSO->m_SRVs;
	m_sharedState.samplers = pDevicePSO->m_Samplers;

	m_sharedState.numSRVs = pDevicePSO->m_NumSRVs;
	m_sharedState.numSamplers = pDevicePSO->m_NumSamplers;
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	auto pResourcesDX11 = reinterpret_cast<const CDeviceResourceSet_DX11*>(pResources);

	if (pResources->GetFlags() & CDeviceResourceSet::EFlags_ForceSetAllState)
	{
		SetResources_All(pResourcesDX11);
	}
	else
	{
		SetResources_RequestedByShaderOnly(pResourcesDX11);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetResources_RequestedByShaderOnly(const CDeviceResourceSet* pResources)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	auto pResourcesDX11 = reinterpret_cast<const CDeviceResourceSet_DX11*>(pResources);

	// Shader stages are ordered by usage-frequency and loop exists according to usage-frequency (VS+PS fast, etc.)
	int validShaderStages = m_sharedState.validShaderStages;
	for (EHWShaderClass shaderClass = eHWSC_Vertex; validShaderStages; shaderClass = EHWShaderClass(shaderClass + 1), validShaderStages >>= 1)
	{
		if (validShaderStages & 1)
		{
			// Bind SRVs
			// if (!pResourcesDX11->compiledSRVs.empty()) // currently this is always the case
			{
				for (int i = 0; i < m_sharedState.numSRVs[shaderClass]; ++i)
				{
					uint8 srvSlot = m_sharedState.srvs[shaderClass][i];
					ID3D11ShaderResourceView* pSrv = pResourcesDX11->compiledTextureSRVs[shaderClass][srvSlot];

					if (pSrv != CDeviceResourceSet_DX11::InvalidPointer)
					{
						if (m_sharedState.shaderResourceView[shaderClass][srvSlot].Set(pSrv))
						{
							BindGraphicsSRV(shaderClass, pSrv, srvSlot);
						}
					}
				}
			}

			// Bind Samplers
			if (!pResourcesDX11->compiledSamplers.empty())
			{
				for (int i = 0; i < m_sharedState.numSamplers[shaderClass]; ++i)
				{
					uint8 samplerSlot = m_sharedState.samplers[shaderClass][i];
					ID3D11SamplerState* pSamplerState = pResourcesDX11->compiledSamplers[shaderClass][samplerSlot];

					if (pSamplerState != CDeviceResourceSet_DX11::InvalidPointer)
					{
						if (m_sharedState.samplerState[shaderClass][samplerSlot].Set(pSamplerState))
						{
							BindGraphicsSampler(shaderClass, pSamplerState, samplerSlot);
						}
					}
				}
			}

			// Bind buffers
			for (int i = 0; i < pResourcesDX11->numCompiledBufferSRVs[shaderClass]; ++i)
			{
				const CDeviceResourceSet_DX11::SCompiledBufferSRV& buffer = pResourcesDX11->compiledBufferSRVs[shaderClass][i];
				BindGraphicsSRV(shaderClass, buffer.pSrv, buffer.slot);
			}

			// Bind constant buffers
			for (int i = 0; i < pResourcesDX11->numCompiledCBs[shaderClass]; ++i)
			{
				const CDeviceResourceSet_DX11::SCompiledConstantBuffer& cb = pResourcesDX11->compiledCBs[shaderClass][i];
				if (m_sharedState.constantBuffer[shaderClass][cb.slot].Set(cb.code))
				{
					BindGraphicsConstantBuffer(shaderClass, cb.pBuffer, cb.slot, cb.offset, cb.size);
				}
			}

			// Bind UAVs
			for (int i = 0; i < pResourcesDX11->numCompiledUAVs; ++i)
			{
				const CDeviceResourceSet_DX11::SCompiledUAV& uav = pResourcesDX11->compiledUAVs[i];
				rd->m_DevMan.BindUAV(uav.shaderType, uav.pUav, 0, uav.slot);
			}
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetResources_All(const CDeviceResourceSet* pResources)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	auto pResourcesDX11 = reinterpret_cast<const CDeviceResourceSet_DX11*>(pResources);

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_NumGfx; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		// Bind SRVs
		for (int slot = 0; slot < pResourcesDX11->compiledTextureSRVs[shaderClass].size(); ++slot)
		{
			ID3D11ShaderResourceView* pSrv = pResourcesDX11->compiledTextureSRVs[shaderClass][slot];

			if (pSrv != CDeviceResourceSet_DX11::InvalidPointer)
			{
				if (m_sharedState.shaderResourceView[shaderClass][slot].Set(pSrv))
				{
					BindGraphicsSRV(shaderClass, pSrv, slot);
				}
			}
		}

		// Bind Samplers
		for (int slot = 0; slot < pResourcesDX11->compiledSamplers[shaderClass].size(); ++slot)
		{
			ID3D11SamplerState* pSamplerState = pResourcesDX11->compiledSamplers[shaderClass][slot];

			if (pSamplerState != CDeviceResourceSet_DX11::InvalidPointer)
			{
				if (m_sharedState.samplerState[shaderClass][slot].Set(pSamplerState))
				{
					BindGraphicsSampler(shaderClass, pSamplerState, slot);
				}
			}
		}

		// Bind buffers
		for (int i = 0; i < pResourcesDX11->numCompiledBufferSRVs[shaderClass]; ++i)
		{
			const CDeviceResourceSet_DX11::SCompiledBufferSRV& buffer = pResourcesDX11->compiledBufferSRVs[shaderClass][i];
			{
				BindGraphicsSRV(shaderClass, buffer.pSrv, buffer.slot);
			}
		}

		// Bind constant buffers
		for (int i = 0; i < pResourcesDX11->numCompiledCBs[shaderClass]; ++i)
		{
			const CDeviceResourceSet_DX11::SCompiledConstantBuffer& cb = pResourcesDX11->compiledCBs[shaderClass][i];
			if (m_sharedState.constantBuffer[shaderClass][cb.slot].Set(cb.code))
			{
				BindGraphicsConstantBuffer(shaderClass, cb.pBuffer, cb.slot, cb.offset, cb.size);
			}
		}

		// Bind UAVs
		for (int i = 0; i < pResourcesDX11->numCompiledUAVs; ++i)
		{
			const CDeviceResourceSet_DX11::SCompiledUAV& uav = pResourcesDX11->compiledUAVs[i];
			rd->m_DevMan.BindUAV(uav.shaderType, uav.pUav, 0, uav.slot);
		}
	}
}
// *INDENT-ON*

void CDeviceGraphicsCommandInterfaceImpl::ApplyDepthStencilState()
{
	if (m_graphicsState.custom.bDepthStencilStateDirty)
	{
		gcpRendD3D->m_DevMan.SetDepthStencilState(m_graphicsState.custom.depthStencilState.cachedValue, m_graphicsState.stencilRef.cachedValue);
		m_graphicsState.custom.bDepthStencilStateDirty = false;
	}
}

void CDeviceGraphicsCommandInterfaceImpl::ApplyRasterizerState()
{
	if (m_graphicsState.custom.bRasterizerStateDirty)
	{
		auto pRasterizerState = m_graphicsState.custom.rasterizerState.cachedValue;

		if (m_graphicsState.custom.depthConstBias != 0.0f || m_graphicsState.custom.depthSlopeBias != 0.0f)
		{
			auto rsDesc = CDeviceStatesManagerDX11::GetInstance()->m_StatesRS[m_graphicsState.custom.rasterizerStateIndex].Desc;
			rsDesc.DepthBias = int(m_graphicsState.custom.depthConstBias);
			rsDesc.SlopeScaledDepthBias = m_graphicsState.custom.depthSlopeBias;
			rsDesc.DepthBiasClamp = m_graphicsState.custom.depthBiasClamp;

			uint32 newRasterizerStateIndex = CDeviceStatesManagerDX11::GetInstance()->GetOrCreateRasterState(rsDesc);
			if (newRasterizerStateIndex != uint32(-1))
			{
				pRasterizerState = CDeviceStatesManagerDX11::GetInstance()->m_StatesRS[newRasterizerStateIndex].pState;
			}
		}

		gcpRendD3D->m_DevMan.SetRasterState(pRasterizerState);
		m_graphicsState.custom.bRasterizerStateDirty = false;
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	// Shader stages are ordered by usage-frequency and loop exists according to usage-frequency (VS+PS fast, etc.)
	int validShaderStages = shaderStages;
	for (EHWShaderClass shaderClass = eHWSC_Vertex; validShaderStages; shaderClass = EHWShaderClass(shaderClass + 1), validShaderStages >>= 1)
	{
		if (validShaderStages & 1)
			SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, shaderClass);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	if (m_sharedState.constantBuffer[shaderClass][shaderSlot].Set(pBuffer->GetCode()))
	{
		buffer_size_t offset, size;
		D3DBuffer* pBufferDX11 = pBuffer->GetD3D(&offset, &size);

		BindGraphicsConstantBuffer(shaderClass, pBufferDX11, shaderSlot, offset, size);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetVertexBuffersImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams)
{
	for (uint32 s = 0; s < numStreams; ++s)
	{
		const CDeviceInputStream& current = vertexStreams[s];

		CRY_ASSERT(current.hStream != ~0u);
		{
			buffer_size_t offset;
			D3DBuffer* buffer = gcpRendD3D.m_DevBufMan.GetD3D(current.hStream, &offset);

			gcpRendD3D.m_DevMan.BindVB(buffer, current.nSlot, offset, current.nStride);
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetIndexBufferImpl(const CDeviceInputStream* indexStream)
{
	const CDeviceInputStream& current = indexStream[0];

	CRY_ASSERT(current.hStream != ~0u);
	{
		buffer_size_t offset;
		D3DBuffer* buffer = gcpRendD3D.m_DevBufMan.GetD3D(current.hStream, &offset);

#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
		gcpRendD3D.m_DevMan.BindIB(buffer, offset, DXGI_FORMAT_R16_UINT);
#else
		gcpRendD3D.m_DevMan.BindIB(buffer, offset, (DXGI_FORMAT)current.nStride);
#endif
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetStencilRefImpl(uint8 stencilRefValue)
{
	m_graphicsState.custom.bDepthStencilStateDirty = true;
}

void CDeviceGraphicsCommandInterfaceImpl::SetDepthBiasImpl(float constBias, float slopeBias, float biasClamp)
{
	m_graphicsState.custom.depthConstBias = constBias;
	m_graphicsState.custom.depthSlopeBias = slopeBias;
	m_graphicsState.custom.depthBiasClamp = biasClamp;

	m_graphicsState.custom.bRasterizerStateDirty = true;
}

void CDeviceGraphicsCommandInterfaceImpl::DrawImpl(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	ApplyDepthStencilState();
	ApplyRasterizerState();

	if (InstanceCount > 1 || StartInstanceLocation > 0)
	{
		rd->m_DevMan.DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}
	else
	{
		rd->m_DevMan.Draw(VertexCountPerInstance, StartVertexLocation);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::DrawIndexedImpl(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
	STATIC_CHECK(false, "NOT IMPLEMENTED");
#endif

	ApplyDepthStencilState();
	ApplyRasterizerState();

	if (InstanceCount > 1 || StartInstanceLocation > 0)
	{
		rd->m_DevMan.DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}
	else
	{
		rd->m_DevMan.DrawIndexed(IndexCountPerInstance, StartIndexLocation, BaseVertexLocation);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DSurface* pView, const float color[4], UINT numRects, const D3D11_RECT* pRects)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

#if (CRY_RENDERER_DIRECT3D >= 111)
	gcpRendD3D->GetDeviceContext().ClearView(pView, color, numRects, pRects);
#else
	CRY_ASSERT(numRects == 0); // not supported on dx11
	gcpRendD3D->GetDeviceContext().ClearRenderTargetView(pView, color);
#endif
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DDepthSurface* pView, int clearFlags, float depth, uint8 stencil, uint32 numRects, const D3D11_RECT* pRects)
{
	CRY_ASSERT(numRects == 0); // not supported on dx11
	gcpRendD3D->GetDeviceContext().ClearDepthStencilView(pView, D3D11_CLEAR_FLAG(clearFlags), depth, stencil);
}

void CDeviceGraphicsCommandInterfaceImpl::BeginOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	gcpRendD3D->GetDeviceContext().Begin(pQuery);
}

void CDeviceGraphicsCommandInterfaceImpl::EndOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	gcpRendD3D->GetDeviceContext().End(pQuery);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceComputeCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceComputePSO* pDevicePSO)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const CDeviceComputePSO_DX11* pDevicePsoDX11 = reinterpret_cast<const CDeviceComputePSO_DX11*>(pDevicePSO);

	// Shader
	const std::array<void*, eHWSC_Num>& shaders = pDevicePsoDX11->m_pDeviceShaders;
	rd->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_CS, (ID3D11Resource*)shaders[eHWSC_Compute]);
}

void CDeviceComputeCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	auto pResourcesDX11 = reinterpret_cast<const CDeviceResourceSet_DX11*>(pResources);

	// Bind SRVs
	for (int slot = 0; slot < pResourcesDX11->compiledTextureSRVs[eHWSC_Compute].size(); ++slot)
	{
		ID3D11ShaderResourceView* pSrv = pResourcesDX11->compiledTextureSRVs[eHWSC_Compute][slot];

		if (pSrv != CDeviceResourceSet_DX11::InvalidPointer)
		{
			if (m_sharedState.shaderResourceView[eHWSC_Compute][slot].Set(pSrv))
			{
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, pSrv, slot);
			}
		}
	}

	// Bind Samplers
	for (int slot = 0; slot < pResourcesDX11->compiledSamplers[eHWSC_Compute].size(); ++slot)
	{
		ID3D11SamplerState* pSamplerState = pResourcesDX11->compiledSamplers[eHWSC_Compute][slot];

		if (pSamplerState != CDeviceResourceSet_DX11::InvalidPointer)
		{
			if (m_sharedState.samplerState[eHWSC_Compute][slot].Set(pSamplerState))
			{
				rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_CS, pSamplerState, slot);
			}
		}
	}

	// Bind buffers
	for (int i = 0; i < pResourcesDX11->numCompiledBufferSRVs[eHWSC_Compute]; ++i)
	{
		const CDeviceResourceSet_DX11::SCompiledBufferSRV& buffer = pResourcesDX11->compiledBufferSRVs[eHWSC_Compute][i];
		rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, buffer.pSrv, buffer.slot);
	}

	// Bind constant buffers
	for (int i = 0; i < pResourcesDX11->numCompiledCBs[eHWSC_Compute]; ++i)
	{
		const CDeviceResourceSet_DX11::SCompiledConstantBuffer& cb = pResourcesDX11->compiledCBs[eHWSC_Compute][i];
		if (m_sharedState.constantBuffer[eHWSC_Compute][cb.slot].Set(cb.code))
		{
			rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_CS, cb.pBuffer, cb.slot, cb.offset, cb.size);
		}
	}

	// Bind UAVs
	for (int i = 0; i < pResourcesDX11->numCompiledUAVs; ++i)
	{
		const CDeviceResourceSet_DX11::SCompiledUAV& uav = pResourcesDX11->compiledUAVs[i];
		rd->m_DevMan.BindUAV(uav.shaderType, uav.pUav, 0, uav.slot);

		m_computeState.custom.boundUAVs[uav.slot] = true;
	}
}

void CDeviceComputeCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	//if (pCmdList->m_CurrentCB[shaderClass][shaderSlot].Set(pBuffer->GetCode()))
	{
		gcpRendD3D->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_CS, pBuffer, shaderSlot);
	}
}

void CDeviceComputeCommandInterfaceImpl::SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
	__debugbreak();
}

void CDeviceComputeCommandInterfaceImpl::DispatchImpl(uint32 X, uint32 Y, uint32 Z)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->m_DevMan.Dispatch(X, Y, Z);

	if (m_computeState.custom.boundUAVs.any())
	{
		for (int i = 0; i < m_computeState.custom.boundUAVs.size(); ++i)
		{
			if (m_computeState.custom.boundUAVs[i])
			{
				rd->m_DevMan.BindUAV(CSubmissionQueue_DX11::TYPE_CS, nullptr, 0, i);
			}
		}

		rd->m_DevMan.CommitDeviceStates();

		m_computeState.custom.boundUAVs = 0;
		memset(m_computeState.pResources, 0x0, sizeof(m_computeState.pResources));
	}
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

#if (CRY_RENDERER_DIRECT3D >= 120)
	gcpRendD3D->GetDeviceContext().ClearRectsUnorderedAccessViewFloat(pView, Values, NumRects, pRects);
#else
	CRY_ASSERT(NumRects == 0); // not supported on dx11
	gcpRendD3D->GetDeviceContext().ClearUnorderedAccessViewFloat(pView, Values);
#endif
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

#if (CRY_RENDERER_DIRECT3D >= 120)
	gcpRendD3D->GetDeviceContext().ClearRectsUnorderedAccessViewUint(pView, Values, NumRects, pRects);
#else
	CRY_ASSERT(NumRects == 0); // not supported on dx11
	gcpRendD3D->GetDeviceContext().ClearUnorderedAccessViewUint(pView, Values);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceNvidiaCommandInterfaceImpl::SetModifiedWModeImpl(bool enabled, uint32 numViewports, const float* pA, const float* pB)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().SetModifiedWMode(enabled, numViewports, pA, pB);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, CDeviceBuffer* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseBuffer(), pSrc->GetBaseBuffer());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DBuffer* pSrc, D3DBuffer* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, CDeviceTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseTexture(), pSrc->GetBaseTexture());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, D3DTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc->GetBaseTexture());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, D3DTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, CDeviceTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseTexture(), pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, CDeviceBuffer* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };

#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseBuffer(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseBuffer(), region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
#else
	rd->GetDeviceContext().CopySubresourcesRegion(pDst->GetBaseBuffer(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseBuffer(), region.SourceOffset.Subresource, &box, region.Extent.Subresources);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DBuffer* pSrc, D3DBuffer* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };

#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst, region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
#else
	rd->GetDeviceContext().CopySubresourcesRegion(pDst, region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Extent.Subresources);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };

	// If CopySubresourceRegion is used with Multisampled or D3D11_BIND_DEPTH_STENCIL Resources, then the whole Subresource must be copied. DstX, DstY, and DstZ must all be 0, while pSrcBox must be NULL.
	D3D11_BOX* pBox = &box;
	if (pSrc->IsResolvable())
	{
		pBox = nullptr;

		CRY_ASSERT(region.DestinationOffset.Left == 0);
		CRY_ASSERT(region.DestinationOffset.Top == 0);
		CRY_ASSERT(region.DestinationOffset.Front == 0);
	}

#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseTexture(), region.SourceOffset.Subresource, pBox, region.Flags, region.Extent.Subresources);
#else
	rd->GetDeviceContext().CopySubresourcesRegion(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseTexture(), region.SourceOffset.Subresource, pBox, region.Extent.Subresources);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };

#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
#else
	rd->GetDeviceContext().CopySubresourcesRegion(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Extent.Subresources);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetD3D(), 0, nullptr, pSrc, 0, 0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceBuffer* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetBaseBuffer(), 0, nullptr, pSrc, memoryLayout.rowStride, memoryLayout.planeStride);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceTexture* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetBaseTexture(), 0, nullptr, pSrc, memoryLayout.rowStride, memoryLayout.planeStride);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CRY_ASSERT((region.Extent.Subresources == 1) && ((region.ResourceOffset.Left == 0) || (CRY_RENDERER_DIRECT3D >= 111))); // TODO: batch
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetD3D(), region.ResourceOffset.Subresource, CRY_RENDERER_DIRECT3D >= 111 ? &box : nullptr, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
#else
	rd->GetDeviceContext().UpdateSubresource(pDst->GetD3D(), region.ResourceOffset.Subresource, CRY_RENDERER_DIRECT3D >= 111 ? &box : nullptr, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceBuffer* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CRY_ASSERT((region.Extent.Subresources == 1) && (CRY_RENDERER_DIRECT3D < 111)); // TODO: batch
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetBaseBuffer(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
#else
	rd->GetDeviceContext().UpdateSubresource(pDst->GetBaseBuffer(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceTexture* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CRY_ASSERT(region.Extent.Subresources == 1); // TODO: batch
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetBaseTexture(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
#else
	rd->GetDeviceContext().UpdateSubresource(pDst->GetBaseTexture(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	assert(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	assert(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, void* pDst, const SResourceMemoryMapping& region)
{
	assert(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryMapping& region)
{
	assert(0);
}
