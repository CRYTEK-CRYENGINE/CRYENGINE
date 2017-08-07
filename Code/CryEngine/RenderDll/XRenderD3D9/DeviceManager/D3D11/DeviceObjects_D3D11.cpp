// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../DeviceObjects.h"
#include "DriverD3D.h"
#include "../../../Common/ReverseDepth.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	#define DX11_COMMANDLIST_REDUNDANT_STATE_FILTERING

class CDeviceGraphicsPSO_DX11 : public CDeviceGraphicsPSO
{
public:
	CDeviceGraphicsPSO_DX11();

	virtual EInitResult Init(const CDeviceGraphicsPSODesc& psoDesc) final;

	_smart_ptr<ID3D11RasterizerState>                 m_pRasterizerState;
	uint32                                            m_RasterizerStateIndex;
	_smart_ptr<ID3D11BlendState>                      m_pBlendState;
	_smart_ptr<ID3D11DepthStencilState>               m_pDepthStencilState;
	_smart_ptr<ID3D11InputLayout>                     m_pInputLayout;

	std::array<void*, eHWSC_Num>                      m_pDeviceShaders;

	std::array<std::array<uint8, MAX_TMU>, eHWSC_Num> m_Samplers;
	std::array<std::array<uint8, MAX_TMU>, eHWSC_Num> m_SRVs;

	std::array<uint8, eHWSC_Num>                      m_NumSamplers;
	std::array<uint8, eHWSC_Num>                      m_NumSRVs;

	// Do we still need these?
	uint64           m_ShaderFlags_RT;
	uint32           m_ShaderFlags_MD;
	uint32           m_ShaderFlags_MDV;

	D3DPrimitiveType m_PrimitiveTopology;
};

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
	m_bValid = false;
	m_nUpdateCount++;

	m_pRasterizerState = nullptr;
	m_pBlendState = nullptr;
	m_pDepthStencilState = nullptr;
	m_pInputLayout = nullptr;

	m_NumSamplers.fill(0);
	m_NumSRVs.fill(0);

	auto hwShaders = SDeviceObjectHelpers::GetShaderInstanceInfo(psoDesc.m_pShader, psoDesc.m_technique, psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, psoDesc.m_bAllowTesselation);

	// validate shaders first
	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (hwShaders[shaderClass].pHwShader && (hwShaders[shaderClass].pHwShaderInstance == NULL || hwShaders[shaderClass].pDeviceShader == NULL))
			return EInitResult::Failure;

		m_pDeviceShaders[shaderClass] = hwShaders[shaderClass].pDeviceShader;
		m_pHwShaderInstances[shaderClass] = hwShaders[shaderClass].pHwShaderInstance;
	}

	D3D11_RASTERIZER_DESC rasterizerDesc;
	D3D11_BLEND_DESC blendDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;

	psoDesc.FillDescs(rasterizerDesc, blendDesc, depthStencilDesc);

	uint32 rasterStateIndex = rd->GetOrCreateRasterState(rasterizerDesc);
	uint32 blendStateIndex = rd->GetOrCreateBlendState(blendDesc);
	uint32 depthStateIndex = rd->GetOrCreateDepthState(depthStencilDesc);

	if (rasterStateIndex == uint32(-1) || blendStateIndex == uint32(-1) || depthStateIndex == uint32(-1))
		return EInitResult::Failure;

	m_pDepthStencilState = rd->m_StatesDP[depthStateIndex].pState;
	m_pRasterizerState = rd->m_StatesRS[rasterStateIndex].pState;
	m_RasterizerStateIndex = rasterStateIndex;
	m_pBlendState = rd->m_StatesBL[blendStateIndex].pState;

	// input layout
	m_pInputLayout = nullptr;
	if (psoDesc.m_VertexFormat != EDefaultInputLayouts::Empty)
	{
		if (psoDesc.m_VertexFormat != EDefaultInputLayouts::Unspecified)
		{
			if (CHWShader_D3D::SHWSInstance* pVsInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(hwShaders[eHWSC_Vertex].pHwShaderInstance))
			{
				uint32 streamMask = psoDesc.CombineVertexStreamMasks(uint8(pVsInstance->m_VStreamMask_Decl), psoDesc.m_ObjectStreamMask);

				m_pInputLayout = CDeviceObjectFactory::LookupInputLayout(CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&pVsInstance->m_Shader, streamMask, 0, 0, nullptr, psoDesc.m_VertexFormat)).second;
			}
		}

		if (!m_pInputLayout)
			return EInitResult::Failure;
	}

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
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

	m_PrimitiveTopology = rd->FX_ConvertPrimitiveType(psoDesc.m_PrimitiveType);

	m_ShaderFlags_RT = psoDesc.m_ShaderFlags_RT;
	m_ShaderFlags_MD = psoDesc.m_ShaderFlags_MD;
	m_ShaderFlags_MDV = psoDesc.m_ShaderFlags_MDV;

	#if defined(ENABLE_PROFILING_CODE)
	m_PrimitiveTypeForProfiling = psoDesc.m_PrimitiveType;
	#endif

	m_bValid = true;
	return EInitResult::Success;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceComputePSO_DX11 : public CDeviceComputePSO
{
public:
	CDeviceComputePSO_DX11();

	virtual bool Init(const CDeviceComputePSODesc& psoDesc) final;

	std::array<void*, eHWSC_Num> m_pDeviceShaders;
};

CDeviceComputePSO_DX11::CDeviceComputePSO_DX11()
{
}

bool CDeviceComputePSO_DX11::Init(const CDeviceComputePSODesc& psoDesc)
{
	CD3D9Renderer* rd = gcpRendD3D;
	m_bValid = false;
	m_nUpdateCount++;

	auto hwShaders = SDeviceObjectHelpers::GetShaderInstanceInfo(psoDesc.m_pShader, psoDesc.m_technique, psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, false);

	if (hwShaders[eHWSC_Compute].pHwShader && (hwShaders[eHWSC_Compute].pHwShaderInstance == NULL || hwShaders[eHWSC_Compute].pDeviceShader == NULL))
		return false;

	m_pDeviceShaders[eHWSC_Compute] = hwShaders[eHWSC_Compute].pDeviceShader;
	m_pHwShaderInstance = hwShaders[eHWSC_Compute].pHwShaderInstance;

	m_bValid = true;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: optimize compiled resource layout for cache friendliness
struct CDeviceResourceSet_DX11 : CDeviceResourceSet
{
	static const void* const InvalidPointer;

	struct SCompiledConstantBuffer
	{
		D3DBuffer* pBuffer;
		uint32 offset;
		uint32 size;
		uint64 code;
		int slot;
	};

	struct SCompiledBufferSRV
	{
		ID3D11ShaderResourceView* pSrv;
		int slot;
	};

	struct SCompiledUAV
	{
		ID3D11UnorderedAccessView* pUav;
		CSubmissionQueue_DX11::SHADER_TYPE shaderType;
		int slot;
	};

	CDeviceResourceSet_DX11(CDeviceResourceSet::EFlags flags)
		: CDeviceResourceSet(flags)
	{}

	virtual bool UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags);

	void         ClearCompiledData();

	// set via reflection from shader
	std::array<std::array<ID3D11ShaderResourceView*, MAX_TMU>, eHWSC_Num>                  compiledTextureSRVs;
	std::array<std::array<ID3D11SamplerState*, MAX_TMU>, eHWSC_Num>                        compiledSamplers;

	// set directly
	std::array<std::array<SCompiledConstantBuffer, eConstantBufferShaderSlot_Count>, eHWSC_Num> compiledCBs;
	std::array<uint8, eHWSC_Num> numCompiledCBs;

	std::array<std::array<SCompiledBufferSRV, ResourceSetBufferCount>, eHWSC_Num> compiledBufferSRVs;
	std::array<uint8, eHWSC_Num> numCompiledBufferSRVs;

	std::array<SCompiledUAV, 8> compiledUAVs;
	uint8 numCompiledUAVs;
};

const void* const CDeviceResourceSet_DX11::InvalidPointer = (const void* const)0xBADA55;

void CDeviceResourceSet_DX11::ClearCompiledData()
{
	// Releasing resources is allowed to be done by any thread, just not concurrently
	for (auto& stage : compiledTextureSRVs)
		stage.fill((ID3D11ShaderResourceView*)InvalidPointer);

	for (auto& stage : compiledSamplers)
		stage.fill((ID3D11SamplerState*)InvalidPointer);

	ZeroMemory(&compiledCBs, sizeof(compiledCBs));
	ZeroMemory(&numCompiledCBs, sizeof(numCompiledCBs));

	ZeroMemory(&compiledBufferSRVs, sizeof(compiledBufferSRVs));
	ZeroMemory(&numCompiledBufferSRVs, sizeof(numCompiledBufferSRVs));

	ZeroMemory(&compiledUAVs, sizeof(compiledUAVs));
	ZeroMemory(&numCompiledUAVs, sizeof(numCompiledUAVs));
}

bool CDeviceResourceSet_DX11::UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	ClearCompiledData();

	for (const auto& it : desc.GetResources())
	{
		const SResourceBindPoint& bindPoint = it.first;
		const SResourceBinding&   resource = it.second;

		CRY_ASSERT(bindPoint.slotNumber < MAX_TMU);

		if (!resource.IsValid())
		{
			CRY_ASSERT_MESSAGE(false, "Invalid resource in resource set desc. Update failed");
			return false;
		}

		switch (bindPoint.slotType)
		{
			case SResourceBindPoint::ESlotType::ConstantBuffer:
			{
				SCompiledConstantBuffer compiledCB;

				buffer_size_t offset, size;

				compiledCB.pBuffer = resource.pConstantBuffer->GetD3D(&offset, &size);
				compiledCB.code = resource.pConstantBuffer->GetCode();
				compiledCB.offset = offset;
				compiledCB.size = size;
				compiledCB.slot = it.first.slotNumber;

				for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
				{
					if (it.first.stages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
						compiledCBs[shaderClass][numCompiledCBs[shaderClass]++] = compiledCB;
				}
			}
			break;

			case SResourceBindPoint::ESlotType::TextureAndBuffer:
			{
				CDeviceResource* pResource = uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsTexture)
					? (CDeviceResource*)resource.pTexture->GetDevTexture()
					: (CDeviceResource*)resource.pBuffer->GetDevBuffer();

				ID3D11ShaderResourceView* pSRV = pResource->LookupSRV(resource.view);

				for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
				{
					if (bindPoint.stages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
						compiledTextureSRVs[shaderClass][bindPoint.slotNumber] = pSRV;
				}
			};
			break;

			case SResourceBindPoint::ESlotType::UnorderedAccessView:
			{
				CRY_ASSERT((bindPoint.stages & (EShaderStage_Vertex | EShaderStage_Domain | EShaderStage_Hull | EShaderStage_Geometry)) == 0);

				CDeviceResource* pResource = uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsTexture)
					? (CDeviceResource*)resource.pTexture->GetDevTexture()
					: (CDeviceResource*)resource.pBuffer->GetDevBuffer();

				ID3D11UnorderedAccessView* pUAV = pResource->LookupUAV(resource.view);

				if (bindPoint.stages & EShaderStage_Compute)
				{
					SCompiledUAV compiledUAV = { pUAV, CSubmissionQueue_DX11::TYPE_CS, bindPoint.slotNumber };
					compiledUAVs[numCompiledUAVs++] = compiledUAV;

					CRY_ASSERT(numCompiledUAVs <= D3D11_PS_CS_UAV_REGISTER_COUNT);
				}

				if (bindPoint.stages & EShaderStage_Pixel)
				{
					SCompiledUAV compiledUAV = { pUAV, CSubmissionQueue_DX11::TYPE_PS, bindPoint.slotNumber };
					compiledUAVs[numCompiledUAVs++] = compiledUAV;
				}

			};
			break;

			case SResourceBindPoint::ESlotType::Sampler:
			{
				ID3D11SamplerState* pSamplerState = CDeviceObjectFactory::LookupSamplerState(resource.samplerState).second;

				for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
				{
					if (bindPoint.stages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
						compiledSamplers[shaderClass][bindPoint.slotNumber] = pSamplerState;
				}
			}
			break;
		}
		
	}

	return true;
}

class CDeviceResourceLayout_DX11 : public CDeviceResourceLayout
{
public:
	CDeviceResourceLayout_DX11(UsedBindSlotSet requiredResourceBindings)
		: CDeviceResourceLayout(requiredResourceBindings)
	{}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeviceRenderPass::UpdateImpl(const CDeviceRenderPassDesc& passDesc)
{
	if (!passDesc.GetDeviceRendertargetViews(m_RenderTargetViews, m_RenderTargetCount))
		return false;

	if (!passDesc.GetDeviceDepthstencilView(m_pDepthStencilView))
		return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(CRY_PLATFORM_ORBIS)

CDeviceTimestampGroup::CDeviceTimestampGroup()
	: m_numTimestamps(0)
	, m_pDisjointQuery(nullptr)
	, m_frequency(0L)
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
	gcpRendD3D->GetDeviceContext().Begin(m_pDisjointQuery);
}

void CDeviceTimestampGroup::EndMeasurement()
{
	gcpRendD3D->GetDeviceContext().End(m_pDisjointQuery);
}

uint32 CDeviceTimestampGroup::IssueTimestamp(void* pCommandList)
{
	assert(m_numTimestamps < m_timestampQueries.size());
	gcpRendD3D->GetDeviceContext().End(m_timestampQueries[m_numTimestamps]);
	return m_numTimestamps++;
}

bool CDeviceTimestampGroup::ResolveTimestamps()
{
	if (m_numTimestamps == 0)
		return true;

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
	if (gcpRendD3D->GetDeviceContext().GetData(m_pDisjointQuery, &disjointData, m_pDisjointQuery->GetDataSize(), 0) == S_OK)
	{
		m_frequency = disjointData.Frequency;
	}
	else
	{
		return false;
	}

	for (int i = m_numTimestamps - 1; i >= 0; i--)
	{
		if (gcpRendD3D->GetDeviceContext().GetData(m_timestampQueries[i], &m_timeValues[i], m_timestampQueries[i]->GetDataSize(), 0) != S_OK)
		{
			return false;
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

	D3DDepthSurface* pDSV = 0;
	D3DSurface*      pRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { 0 };
	D3DUAV*          pUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT        ] = { 0 };

	if (bOutputMergerOnly)
		rd->GetDeviceContext().OMSetRenderTargets/*AndUnorderedAccessViews*/(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, pRTVs, pDSV/*, 0, D3D11_PS_CS_UAV_REGISTER_COUNT, pUAVs, nullptr*/);
	else
		rd->GetDeviceContext().ClearState();
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
	m_graphicsState.custom.depthStencilState = nullptr;
	m_graphicsState.custom.rasterizerState = nullptr;
	m_graphicsState.custom.rasterizerStateIndex = 0;	
	m_graphicsState.custom.blendState = nullptr;
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

	// Shaders
	const std::array<void*, eHWSC_Num>& shaders = pDevicePSO->m_pDeviceShaders;
	if (m_sharedState.shader[eHWSC_Vertex].Set(shaders[eHWSC_Vertex]))     rd->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_VS, (ID3D11Resource*)shaders[eHWSC_Vertex]);
	if (m_sharedState.shader[eHWSC_Pixel].Set(shaders[eHWSC_Pixel]))       rd->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_PS, (ID3D11Resource*)shaders[eHWSC_Pixel]);
	if (m_sharedState.shader[eHWSC_Geometry].Set(shaders[eHWSC_Geometry])) rd->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_GS, (ID3D11Resource*)shaders[eHWSC_Geometry]);
	if (m_sharedState.shader[eHWSC_Domain].Set(shaders[eHWSC_Domain]))     rd->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_DS, (ID3D11Resource*)shaders[eHWSC_Domain]);
	if (m_sharedState.shader[eHWSC_Hull].Set(shaders[eHWSC_Hull]))         rd->m_DevMan.BindShader(CSubmissionQueue_DX11::TYPE_HS, (ID3D11Resource*)shaders[eHWSC_Hull]);

	// input layout and topology
	if (m_graphicsState.custom.inputLayout.Set(pDevicePSO->m_pInputLayout.get()))   rd->m_DevMan.BindVtxDecl(pDevicePSO->m_pInputLayout);
	if (m_graphicsState.custom.topology.Set(pDevicePSO->m_PrimitiveTopology))       rd->m_DevMan.BindTopology(pDevicePSO->m_PrimitiveTopology);

	// update valid shader mask
	m_sharedState.validShaderStages = EShaderStage_None;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Vertex])   m_sharedState.validShaderStages |= EShaderStage_Vertex;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Pixel])    m_sharedState.validShaderStages |= EShaderStage_Pixel;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Geometry]) m_sharedState.validShaderStages |= EShaderStage_Geometry;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Domain])   m_sharedState.validShaderStages |= EShaderStage_Domain;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Hull])     m_sharedState.validShaderStages |= EShaderStage_Hull;

	m_sharedState.srvs = pDevicePSO->m_SRVs;
	m_sharedState.samplers = pDevicePSO->m_Samplers;

	m_sharedState.numSRVs = pDevicePSO->m_NumSRVs;
	m_sharedState.numSamplers = pDevicePSO->m_NumSamplers;

	// TheoM TODO: REMOVE once shaders are set up completely via pso
	rd->m_RP.m_FlagsShader_RT = pDevicePSO->m_ShaderFlags_RT;
	rd->m_RP.m_FlagsShader_MD = pDevicePSO->m_ShaderFlags_MD;
	rd->m_RP.m_FlagsShader_MDV = pDevicePSO->m_ShaderFlags_MDV;
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

// *INDENT-OFF*
void BindGraphicsSRV(EHWShaderClass shaderClass, ID3D11ShaderResourceView* pSrv, uint32 slot)
{
	CD3D9Renderer *const __restrict rd = gcpRendD3D;

	switch (shaderClass)
	{
	case eHWSC_Vertex:   rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_VS, pSrv, slot); break;
	case eHWSC_Pixel:    rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_PS, pSrv, slot); break;
	case eHWSC_Geometry: rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_GS, pSrv, slot); break;
	case eHWSC_Domain:   rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_DS, pSrv, slot); break;
	case eHWSC_Hull:     rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_HS, pSrv, slot); break;
	default:
		CRY_ASSERT(false);
	}
}

void BindGraphicsSampler(EHWShaderClass shaderClass, ID3D11SamplerState* pSamplerState, uint32 slot)
{
	CD3D9Renderer *const __restrict rd = gcpRendD3D;

	switch (shaderClass)
	{
	case eHWSC_Vertex:   rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_VS, pSamplerState, slot); break;
	case eHWSC_Pixel:    rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_PS, pSamplerState, slot); break;
	case eHWSC_Geometry: rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_GS, pSamplerState, slot); break;
	case eHWSC_Domain:   rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_DS, pSamplerState, slot); break;
	case eHWSC_Hull:     rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_HS, pSamplerState, slot); break;
	default:
		CRY_ASSERT(false);
	}
}

void BindGraphicsConstantBuffer(EHWShaderClass shaderClass, D3DBuffer* pBuffer, uint32 slot, uint32 offset, uint32 size)
{
	CD3D9Renderer *const __restrict rd = gcpRendD3D;

	switch (shaderClass)
	{
	case eHWSC_Vertex:   rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, pBuffer, slot, offset, size); break;
	case eHWSC_Pixel:    rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_PS, pBuffer, slot, offset, size); break;
	case eHWSC_Geometry: rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_GS, pBuffer, slot, offset, size); break;
	case eHWSC_Domain:   rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_DS, pBuffer, slot, offset, size); break;
	case eHWSC_Hull:     rd->m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_HS, pBuffer, slot, offset, size); break;
	default:
		CRY_ASSERT(false);
	}
}
// *INDENT-ON*

void CDeviceGraphicsCommandInterfaceImpl::SetResources_RequestedByShaderOnly(const CDeviceResourceSet* pResources)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	auto pResourcesDX11 = reinterpret_cast<const CDeviceResourceSet_DX11*>(pResources);

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (m_sharedState.validShaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
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

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
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
			auto rsDesc = gcpRendD3D->m_StatesRS[m_graphicsState.custom.rasterizerStateIndex].Desc;
			rsDesc.DepthBias = int(m_graphicsState.custom.depthConstBias);
			rsDesc.SlopeScaledDepthBias = m_graphicsState.custom.depthSlopeBias;
			rsDesc.DepthBiasClamp = m_graphicsState.custom.depthBiasClamp;

			uint32 newRasterizerStateIndex = gcpRendD3D->GetOrCreateRasterState(rsDesc);
			if (newRasterizerStateIndex >= 0)
			{
				pRasterizerState = gcpRendD3D->m_StatesRS[newRasterizerStateIndex].pState;
			}
		}

		gcpRendD3D->m_DevMan.SetRasterState(pRasterizerState);
		m_graphicsState.custom.bRasterizerStateDirty = false;
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (shaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
		{
			SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, shaderClass);
		}
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
	return gcpRendD3D->GetDeviceContext().ClearRenderTargetView(pView, color);
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
#if (CRY_RENDERER_DIRECT3D < 111)
	if (NumRects) __debugbreak();
#endif
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().ClearUnorderedAccessViewFloat(pView, Values);
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
#if (CRY_RENDERER_DIRECT3D < 111)
	if (NumRects) __debugbreak();
#endif
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().ClearUnorderedAccessViewUint(pView, Values);
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
	rd->GetDeviceContext().CopySubresourcesRegion (pDst->GetBaseBuffer(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseBuffer(), region.SourceOffset.Subresource, &box              , region.Extent.Subresources);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DBuffer* pSrc, D3DBuffer* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };

#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst, region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
#else
	rd->GetDeviceContext().CopySubresourcesRegion (pDst, region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box              , region.Extent.Subresources);
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

		CRY_ASSERT(region.DestinationOffset.Left  == 0);
		CRY_ASSERT(region.DestinationOffset.Top   == 0);
		CRY_ASSERT(region.DestinationOffset.Front == 0);
	}

#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseTexture(), region.SourceOffset.Subresource, pBox, region.Flags, region.Extent.Subresources);
#else
	rd->GetDeviceContext().CopySubresourcesRegion (pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseTexture(), region.SourceOffset.Subresource, pBox              , region.Extent.Subresources);
#endif
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };

#if (CRY_RENDERER_DIRECT3D >= 111)
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
#else
	rd->GetDeviceContext().CopySubresourcesRegion (pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box              , region.Extent.Subresources);
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
	rd->GetDeviceContext().UpdateSubresource (pDst->GetD3D(), region.ResourceOffset.Subresource, CRY_RENDERER_DIRECT3D >= 111 ? &box : nullptr, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride);
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
	rd->GetDeviceContext().UpdateSubresource (pDst->GetBaseBuffer(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride);
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
	rd->GetDeviceContext().UpdateSubresource (pDst->GetBaseTexture(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride);
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceObjectFactory::CDeviceObjectFactory()
	: m_fence_handle(0)
{
	memset(m_NullResources, 0, sizeof(m_NullResources));

	m_frameFenceCounter = 0;
	m_completedFrameFenceCounter = 0;
	for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_frameFences); i++)
		m_frameFences[i] = NULL;

	m_pCoreCommandList.reset(new CDeviceCommandList());
}

void CDeviceObjectFactory::AssignDevice(D3DDevice* pDevice)
{
#if CRY_PLATFORM_DURANGO
	D3D11_DMA_ENGINE_CONTEXT_DESC dmaDesc = { 0 };
	dmaDesc.CreateFlags = D3D11_DMA_ENGINE_CONTEXT_CREATE_SDMA_1;
	dmaDesc.RingBufferSizeBytes = 0;

	gcpRendD3D->GetPerformanceDevice().CreateDmaEngineContext(&dmaDesc, &m_pDMA1);

	UINT poolMemModel =
#if defined(TEXTURES_IN_CACHED_MEM)
		D3D11_GRAPHICS_MEMORY_ACCESS_CPU_CACHE_NONCOHERENT_GPU_READONLY
#else
		D3D11_GRAPHICS_MEMORY_ACCESS_CPU_WRITECOMBINE_NONCOHERENT
#endif
		;

	m_texturePool.Init(
		CRenderer::GetTexturesStreamPoolSize() * 1024 * 1024,
		512 * 1024 * 1024,
		CRenderer::GetTexturesStreamPoolSize() * 1024 * 1024,
		poolMemModel,
		false);

	m_textureStagingRing.Init(m_pDMA1, 128 * 1024 * 1024);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSOImpl(const CDeviceGraphicsPSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceGraphicsPSO_DX11>();
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSOImpl(const CDeviceComputePSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceComputePSO_DX11>();
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceResourceSetPtr CDeviceObjectFactory::CreateResourceSet(CDeviceResourceSet::EFlags flags) const
{
	return std::make_shared<CDeviceResourceSet_DX11>(flags);
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const
{
	return std::make_shared<CDeviceResourceLayout_DX11>(resourceLayoutDesc.GetRequiredResourceBindings());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Acquire one or more command-lists which are independent of the core command-list
// Only one thread is allowed to call functions on this command-list (DX12 restriction).
// The thread that gets the permition is the one calling Begin() on it AFAIS
CDeviceCommandListUPtr CDeviceObjectFactory::AcquireCommandList(EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// TODO: implement deferred contexts
	__debugbreak();

	return CryMakeUnique<CDeviceCommandList>();
}

std::vector<CDeviceCommandListUPtr> CDeviceObjectFactory::AcquireCommandLists(uint32 listCount, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// TODO: implement deferred contexts
	__debugbreak();

	return std::vector<CDeviceCommandListUPtr>(size_t(listCount));
}

void CDeviceObjectFactory::ForfeitCommandList(CDeviceCommandListUPtr pCommandList, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// TODO: implement deferred contexts
	__debugbreak();

	// pContext->ExecuteCommandList(ID3D11CommandList)
}

void CDeviceObjectFactory::ForfeitCommandLists(std::vector<CDeviceCommandListUPtr> pCommandLists, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// TODO: implement deferred contexts
	__debugbreak();

	// pContext->ExecuteCommandList(ID3D11CommandList)
}

void CDeviceObjectFactory::ReleaseResourcesImpl()
{

}

////////////////////////////////////////////////////////////////////////////
UINT64 CDeviceObjectFactory::QueryFormatSupport(D3DFormat Format)
{
	CD3D9Renderer* rd = gcpRendD3D;

	UINT nOptions;
	HRESULT hr = rd->GetDevice().CheckFormatSupport(Format, &nOptions);
	if (SUCCEEDED(hr))
	{
		// *INDENT-OFF*
		return
			(nOptions & D3D11_FORMAT_SUPPORT_BUFFER                      ? FMTSUPPORT_BUFFER                      : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE1D                   ? FMTSUPPORT_TEXTURE1D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE2D                   ? FMTSUPPORT_TEXTURE2D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE3D                   ? FMTSUPPORT_TEXTURE3D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURECUBE                 ? FMTSUPPORT_TEXTURECUBE                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER            ? FMTSUPPORT_IA_VERTEX_BUFFER            : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER             ? FMTSUPPORT_IA_INDEX_BUFFER             : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SO_BUFFER                   ? FMTSUPPORT_SO_BUFFER                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MIP                         ? FMTSUPPORT_MIP                         : 0) |
//			(nOptions & D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT      ? FMTSUPPORT_SRGB                        : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_LOAD                 ? FMTSUPPORT_SHADER_LOAD                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_GATHER               ? FMTSUPPORT_SHADER_GATHER               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON    ? FMTSUPPORT_SHADER_GATHER_COMPARISON    : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE               ? FMTSUPPORT_SHADER_SAMPLE               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON    ? FMTSUPPORT_SHADER_SAMPLE_COMPARISON    : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW ? FMTSUPPORT_TYPED_UNORDERED_ACCESS_VIEW : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL               ? FMTSUPPORT_DEPTH_STENCIL               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_RENDER_TARGET               ? FMTSUPPORT_RENDER_TARGET               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_BLENDABLE                   ? FMTSUPPORT_BLENDABLE                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_DISPLAY                     ? FMTSUPPORT_DISPLAYABLE                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD            ? FMTSUPPORT_MULTISAMPLE_LOAD            : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE         ? FMTSUPPORT_MULTISAMPLE_RESOLVE         : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET    ? FMTSUPPORT_MULTISAMPLE_RENDERTARGET    : 0);
		// *INDENT-ON*
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Fence API

#if !CRY_PLATFORM_ORBIS && !(CRY_PLATFORM_DURANGO && BUFFER_ENABLE_DIRECT_ACCESS)
HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	D3D11_QUERY_DESC QDesc;
	QDesc.Query = D3D11_QUERY_EVENT;
	QDesc.MiscFlags = 0;
	ID3D11Query* d3d_query = nullptr;
	hr = gcpRendD3D->GetDevice().CreateQuery(&QDesc, &d3d_query);
	if (!FAILED(hr))
	{
		query = reinterpret_cast<DeviceFenceHandle>(d3d_query);
		IssueFence(query);
	}
	return hr;
}

HRESULT CDeviceObjectFactory::ReleaseFence(DeviceFenceHandle query)
{
	HRESULT hr = S_FALSE;
	ID3D11Query* d3d_query = reinterpret_cast<ID3D11Query*>(query);
	SAFE_RELEASE(d3d_query);
	hr = S_OK;
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = S_FALSE;
	ID3D11Query* d3d_query = reinterpret_cast<ID3D11Query*>(query);
	if (d3d_query)
	{
		gcpRendD3D->GetDeviceContext().End(d3d_query);
		hr = S_OK;
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = S_FALSE;
	ID3D11Query* d3d_query = reinterpret_cast<ID3D11Query*>(query);
	if (d3d_query)
	{
		CRY_ASSERT(!flush || !gRenDev->m_pRT || gRenDev->m_pRT->IsRenderThread()); // Can only flush from render thread

		BOOL bQuery = false;
		do
		{
			hr = gcpRendD3D->GetDeviceContext().GetData(d3d_query, (void*)&bQuery, sizeof(BOOL), flush ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH);
		} while (block && hr != S_OK);
	}
	return hr;
}
#elif CRY_PLATFORM_ORBIS
HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	query = reinterpret_cast<DeviceFenceHandle>(new GpuSyncId);
	hr = query ? S_OK : E_FAIL;
	if (!FAILED(hr))
	{
		IssueFence(query);
	}
	return hr;
}

HRESULT CDeviceObjectFactory::ReleaseFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : S_FALSE;
	delete reinterpret_cast<GpuSyncId*>(query); // TODO: CHECK THIS
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : E_FAIL;
	GpuSyncId* handle = reinterpret_cast<GpuSyncId*>(query);
	if (handle)
	{
		*handle = DXOrbis::GetLastDrawCall();
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = E_FAIL;
	GpuSyncId* handle = reinterpret_cast<GpuSyncId*>(query);
	if (handle)
	{
		SSerialCompare<GpuSyncId> serialCompare;
		if (block)
		{
			DXOrbis::Device()->WaitForGPUDrawCallComplete(*handle);
			hr = S_OK;
		}
		else if (serialCompare(*handle, DXOrbis::Device()->GetLastCompletedDrawCall()))
		{
			hr = S_OK;
		}
		else
		{
			hr = S_FALSE;
		}
	}
	return hr;
}
#endif

////////////////////////////////////////////////////////////////////////////
// SamplerState API

static inline D3D11_TEXTURE_ADDRESS_MODE sAddressMode(ESamplerAddressMode nAddress)
{
	static_assert((eSamplerAddressMode_Wrap   + 1) == D3D11_TEXTURE_ADDRESS_WRAP  , "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Mirror + 1) == D3D11_TEXTURE_ADDRESS_MIRROR, "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Clamp  + 1) == D3D11_TEXTURE_ADDRESS_CLAMP , "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Border + 1) == D3D11_TEXTURE_ADDRESS_BORDER, "AddressMode enum not mappable to D3D11 enum by arithmetic");

	assert(nAddress >= eSamplerAddressMode_Wrap && nAddress <= eSamplerAddressMode_Border);
	return D3D11_TEXTURE_ADDRESS_MODE(nAddress + 1);
}

CDeviceSamplerState* CDeviceObjectFactory::CreateSamplerState(const SSamplerState& pState)
{
	D3D11_SAMPLER_DESC Desc;
	ZeroStruct(Desc);
	CDeviceSamplerState* pSamp = NULL;
	// AddressMode of 0 is INVALIDARG
	Desc.AddressU = sAddressMode(ESamplerAddressMode(pState.m_nAddressU));
	Desc.AddressV = sAddressMode(ESamplerAddressMode(pState.m_nAddressV));
	Desc.AddressW = sAddressMode(ESamplerAddressMode(pState.m_nAddressW));
	ColorF col((uint32)pState.m_dwBorderColor);
	Desc.BorderColor[0] = col.r;
	Desc.BorderColor[1] = col.g;
	Desc.BorderColor[2] = col.b;
	Desc.BorderColor[3] = col.a;
	if (pState.m_bComparison)
		Desc.ComparisonFunc = D3D11_COMPARISON_LESS;
	else
		Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	Desc.MaxAnisotropy = 1;
	Desc.MinLOD = 0;
	if (pState.m_nMipFilter == FILTER_NONE)
		Desc.MaxLOD = 0.0f;
	else
		Desc.MaxLOD = 100.0f;

	Desc.MipLODBias = pState.m_fMipLodBias;

	if (pState.m_bComparison)
	{
		if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && pState.m_nMipFilter == FILTER_LINEAR || pState.m_nMinFilter == FILTER_TRILINEAR || pState.m_nMagFilter == FILTER_TRILINEAR)
		{
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		}
		else if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT) || pState.m_nMinFilter == FILTER_BILINEAR || pState.m_nMagFilter == FILTER_BILINEAR)
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		else if (pState.m_nMinFilter == FILTER_POINT && pState.m_nMagFilter == FILTER_POINT && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT))
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		else if (pState.m_nMinFilter >= FILTER_ANISO2X && pState.m_nMagFilter >= FILTER_ANISO2X && pState.m_nMipFilter >= FILTER_ANISO2X)
		{
			Desc.Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
			Desc.MaxAnisotropy = pState.m_nAnisotropy;
		}
	}
	else
	{
		if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && pState.m_nMipFilter == FILTER_LINEAR || pState.m_nMinFilter == FILTER_TRILINEAR || pState.m_nMagFilter == FILTER_TRILINEAR)
		{
			Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		else if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT) || pState.m_nMinFilter == FILTER_BILINEAR || pState.m_nMagFilter == FILTER_BILINEAR)
			Desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		else if (pState.m_nMinFilter == FILTER_POINT && pState.m_nMagFilter == FILTER_POINT && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT))
			Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		else if (pState.m_nMinFilter >= FILTER_ANISO2X && pState.m_nMagFilter >= FILTER_ANISO2X && pState.m_nMipFilter >= FILTER_ANISO2X)
		{
			Desc.Filter = D3D11_FILTER_ANISOTROPIC;
			Desc.MaxAnisotropy = pState.m_nAnisotropy;
		}
		else
			assert(0);
	}

	HRESULT hr = gcpRendD3D->GetDevice().CreateSamplerState(&Desc, &pSamp);
	if (SUCCEEDED(hr))
		return pSamp;
	else
		assert(0);
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////
// InputLayout API

CDeviceInputLayout* CDeviceObjectFactory::CreateInputLayout(const SInputLayout& pLayout)
{
	CDeviceInputLayout* Layout = nullptr;
	if (!pLayout.m_pVertexShader || !pLayout.m_pVertexShader->m_pShaderData)
		return Layout;

	const int   nSize = pLayout.m_pVertexShader->m_nDataSize;
	const void* pVSData = pLayout.m_pVertexShader->m_pShaderData;

	HRESULT hr = E_FAIL;
	if (FAILED(hr = gcpRendD3D->GetDevice().CreateInputLayout(&pLayout.m_Declaration[0], pLayout.m_Declaration.size(), pVSData, nSize, &Layout)))
	{
		return Layout;
	}

	return Layout;
}

////////////////////////////////////////////////////////////////////////////
// Low-level resource management API (TODO: remove D3D-dependency by abstraction)

void CDeviceObjectFactory::AllocateNullResources()
{
}

void CDeviceObjectFactory::ReleaseNullResources()
{
}

//=============================================================================

#ifdef DEVRES_USE_STAGING_POOL
D3DResource* CDeviceObjectFactory::AllocateStagingResource(D3DResource* pForTex, bool bUpload, void*& pMappedAddress)
{
	D3D11_TEXTURE2D_DESC Desc;
	memset(&Desc, 0, sizeof(Desc));

	((D3DTexture*)pForTex)->GetDesc(&Desc);
	Desc.Usage = bUpload ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_STAGING;
	Desc.CPUAccessFlags = bUpload ? D3D11_CPU_ACCESS_WRITE : D3D11_CPU_ACCESS_READ;
	Desc.BindFlags = bUpload ? D3D11_BIND_SHADER_RESOURCE : 0;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.MiscFlags = 0;
	StagingPoolVec::iterator it = std::find(m_stagingPool.begin(), m_stagingPool.end(), Desc);

	if (it == m_stagingPool.end())
	{
		D3DTexture* pStagingTexture = NULL;

		gcpRendD3D->GetDevice().CreateTexture2D(&Desc, NULL, &pStagingTexture);

#ifndef _RELEASE
		if (pStagingTexture)
		{
			D3D11_TEXTURE2D_DESC stagingDesc;
			memset(&stagingDesc, 0, sizeof(stagingDesc));

			pStagingTexture->GetDesc(&stagingDesc);
			stagingDesc.Usage = bUpload ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_STAGING;
			stagingDesc.CPUAccessFlags = bUpload ? D3D11_CPU_ACCESS_WRITE : D3D11_CPU_ACCESS_READ;
			stagingDesc.BindFlags = bUpload ? D3D11_BIND_SHADER_RESOURCE : 0;

			if (memcmp(&stagingDesc, &Desc, sizeof(Desc)) != 0)
			{
				assert(0);
			}
		}
#endif

		return pStagingTexture;
	}
	else
	{
		D3DTexture* pStagingResource = NULL;

		pStagingResource = it->pStagingResource;
		m_stagingPool.erase(it);

		return pStagingResource;
	}
}

void CDeviceObjectFactory::ReleaseStagingResource(D3DResource* pStagingRes)
{
	D3D11_TEXTURE2D_DESC Desc;
	memset(&Desc, 0, sizeof(Desc));

	((D3DTexture*)pStagingRes)->GetDesc(&Desc);

	StagingTextureDef def;
	def.desc = Desc;
	def.pStagingResource = (D3DTexture*)pStagingRes;
	m_stagingPool.push_back(def);
}
#endif

//=============================================================================

HRESULT CDeviceObjectFactory::Create2DTexture(uint32 nWidth, uint32 nHeight, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload, int32 nESRAMOffset)
{
	HRESULT hr = S_OK;

	// The format(0x2d, D24_UNORM_S8_UINT) cannot be bound as a ShaderResource or cast to a format that could be bound as a ShaderResource.
	// Therefore this format cannot support D3D11_BIND_SHADER_RESOURCE.Specifiying the format R24G8_TYPELESS instead would solve this issue.
	if ((nUsage & (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE)) == (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE))
		Format = DeviceFormats::ConvertToTypeless(Format);

	D3D11_TEXTURE2D_DESC Desc;
	ZeroStruct(Desc);
	Desc.Width = nWidth;
	Desc.Height = nHeight;
	Desc.MipLevels = nMips;
	Desc.SampleDesc.Count = pPayload ? pPayload->m_nDstMSAASamples : 1;
	Desc.SampleDesc.Quality = pPayload ? pPayload->m_nDstMSAAQuality : 0;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.ArraySize = nArraySize;
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	if (nESRAMOffset != SKIP_ESRAM)
	{
		Desc.MiscFlags |= D3D11X_RESOURCE_MISC_ESRAM_RESIDENT;
		Desc.ESRAMOffsetBytes = (UINT)nESRAMOffset;
	}
#endif

#if CRY_PLATFORM_DURANGO
	if (InPlaceConstructable(Desc, nUsage))
	{
		hr = CreateInPlaceTexture2D(Desc, nUsage, pPayload, *ppDevTexture);
	}
	else
#endif
	{
		hr = Create2DTexture(Desc, nUsage, cClearValue, ppDevTexture, pPayload);
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(uint32 nSize, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	HRESULT hr = S_OK;

	// The format(0x2d, D24_UNORM_S8_UINT) cannot be bound as a ShaderResource or cast to a format that could be bound as a ShaderResource.
	// Therefore this format cannot support D3D11_BIND_SHADER_RESOURCE.Specifiying the format R24G8_TYPELESS instead would solve this issue.
	if ((nUsage & (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE)) == (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE))
		Format = DeviceFormats::ConvertToTypeless(Format);

	D3D11_TEXTURE2D_DESC Desc;
	ZeroStruct(Desc);
	Desc.Width = nSize;
	Desc.Height = nSize;
	Desc.MipLevels = nMips;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.ArraySize = nArraySize; assert(!(nArraySize % 6));
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage) | D3D11_RESOURCE_MISC_TEXTURECUBE;

#if CRY_PLATFORM_DURANGO
	if (InPlaceConstructable(Desc, nUsage))
	{
		hr = CreateInPlaceTexture2D(Desc, nUsage, pPayload, *ppDevTexture);
	}
	else
#endif
	{
		hr = CreateCubeTexture(Desc, nUsage, cClearValue, ppDevTexture, pPayload);
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
	HRESULT hr = S_OK;

	// The format(0x2d, D24_UNORM_S8_UINT) cannot be bound as a ShaderResource or cast to a format that could be bound as a ShaderResource.
	// Therefore this format cannot support D3D11_BIND_SHADER_RESOURCE.Specifiying the format R24G8_TYPELESS instead would solve this issue.
	if ((nUsage & (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE)) == (BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE))
		Format = DeviceFormats::ConvertToTypeless(Format);

	D3D11_TEXTURE3D_DESC Desc;
	ZeroStruct(Desc);
	Desc.Width = nWidth;
	Desc.Height = nHeight;
	Desc.Depth = nDepth;
	Desc.MipLevels = nMips;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

	return CreateVolumeTexture(Desc, nUsage, cClearValue, ppDevTexture, pTI);
}

HRESULT CDeviceObjectFactory::Create2DTexture(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3DTexture* pD3DTex = NULL; assert(!pPayload || (((Desc.MipLevels * Desc.ArraySize) <= g_nD3D10MaxSupportedSubres)));
	HRESULT hr = gcpRendD3D->GetDevice().CreateTexture2D(&Desc, ConvertToDX11Data(Desc.MipLevels, pPayload, SRD), &pD3DTex);

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, 1, Desc.MipLevels, Desc.ArraySize, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3DCubeTexture* pD3DTex = NULL; assert(!pPayload || (((Desc.MipLevels * Desc.ArraySize) <= g_nD3D10MaxSupportedSubres) && !(Desc.ArraySize % 6)));
	HRESULT hr = gcpRendD3D->GetDevice().CreateTexture2D(&Desc, ConvertToDX11Data(Desc.MipLevels * Desc.ArraySize, pPayload, SRD), &pD3DTex);

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		pDeviceTexture->m_bCube = true;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, 1, Desc.MipLevels, Desc.ArraySize, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(const D3D11_TEXTURE3D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3DVolumeTexture* pD3DTex = NULL;
	HRESULT hr = gcpRendD3D->GetDevice().CreateTexture3D(&Desc, ConvertToDX11Data(Desc.MipLevels, pPayload, SRD), &pD3DTex);

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, Desc.Depth, Desc.MipLevels, 1, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateBuffer(
	buffer_size_t nSize
	, buffer_size_t elemSize
	, uint32 nUsage
	, uint32 nBindFlags
	, D3DBuffer** ppBuff
	, const void* pData)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "CreateBuffer");
	HRESULT hr = S_OK;

#ifndef _RELEASE
	// ToDo verify that the usage and bindflags are correctly set (e.g certain
	// bit groups are mutually exclusive).
#endif

	D3D11_BUFFER_DESC BufDesc;
	ZeroStruct(BufDesc);

#if CRY_PLATFORM_DURANGO && BUFFER_USE_STAGED_UPDATES == 0
	if (nUsage & USAGE_DIRECT_ACCESS)
	{
	//	if (nUsage & HEAP_STAGING)
	//	{
	//		CryFatalError("staging buffers not supported if BUFFER_USE_STAGED_UPDATES not defined");
	//	}

		BufDesc.StructureByteStride = elemSize;
		BufDesc.ByteWidth = nSize;
		if ((nUsage & USAGE_CPU_WRITE))
			BufDesc.ByteWidth = CDeviceBufferManager::AlignElementCountForStreaming(nSize, elemSize) * elemSize;

		BufDesc.Usage = (D3D11_USAGE)D3D11_USAGE_DEFAULT;
		BufDesc.MiscFlags = 0;
		BufDesc.CPUAccessFlags = 0;
		BufDesc.BindFlags = ConvertToDX11BindFlags(nBindFlags);
		if (nBindFlags & (BIND_STREAM_OUTPUT | BIND_RENDER_TARGET | BIND_DEPTH_STENCIL))
		{
			CryFatalError("trying to create (currently) unsupported buffer type");
		}

		void* BufBasePtr;

		if (!IsAligned(nSize * elemSize, 4096))
		{
			CryFatalError("Memory Allocation Size for Direct Video Memory Access must be a multiple of 4 KB but the supplied size is %u", nSize * elemSize);
		}

		D3D11_GRAPHICS_MEMORY_ACCESS_FLAG access_flag = D3D11_GRAPHICS_MEMORY_ACCESS_CPU_WRITECOMBINE_NONCOHERENT;
		if ((nUsage & (USAGE_DIRECT_ACCESS_CPU_COHERENT | USAGE_DIRECT_ACCESS_GPU_COHERENT)) ==
			(USAGE_DIRECT_ACCESS_CPU_COHERENT | USAGE_DIRECT_ACCESS_GPU_COHERENT))
		{
			access_flag = D3D11_GRAPHICS_MEMORY_ACCESS_CPU_CACHE_COHERENT;
		}
		{
			hr = D3DAllocateGraphicsMemory(
				nSize * elemSize,
				0, 0
				, access_flag
				, &BufBasePtr);
		}
		assert(hr == S_OK);

		hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementBuffer(&BufDesc, BufBasePtr, ppBuff);
		assert(hr == S_OK);

		UINT size = sizeof(BufBasePtr);
		hr = (*ppBuff)->SetPrivateData(BufferPointerGuid, size, &BufBasePtr);
		assert(hr == S_OK);

		if (pData)
		{
			StreamBufferData(BufBasePtr, pData, nSize * elemSize);
		}

		return hr;
	}
#endif

	BufDesc.StructureByteStride = elemSize;
	BufDesc.ByteWidth = nSize * elemSize;
	if ((nUsage & USAGE_CPU_WRITE))
		BufDesc.ByteWidth = CDeviceBufferManager::AlignElementCountForStreaming(nSize, elemSize) * elemSize;
	
	BufDesc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	BufDesc.Usage = ConvertToDX11Usage(nUsage);
	BufDesc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

#if !CRY_RENDERER_OPENGL
	if (BufDesc.Usage != D3D11_USAGE_STAGING)
#endif
	{
		BufDesc.BindFlags = ConvertToDX11BindFlags(nBindFlags);
		if (nBindFlags & (BIND_STREAM_OUTPUT | BIND_RENDER_TARGET | BIND_DEPTH_STENCIL))
		{
			CryFatalError("trying to create (currently) unsupported buffer type");
		}
	}
#if !CRY_RENDERER_OPENGL
	else
	{
		BufDesc.BindFlags = 0;
	}
#endif

	D3D11_SUBRESOURCE_DATA* pSRD = NULL;
	D3D11_SUBRESOURCE_DATA SRD;
	if (pData)
	{
		pSRD = &SRD;

		SRD.pSysMem = pData;
		SRD.SysMemPitch = BufDesc.ByteWidth;
		SRD.SysMemSlicePitch = BufDesc.ByteWidth;
	}

	hr = gcpRendD3D->GetDevice().CreateBuffer(&BufDesc, pSRD, ppBuff);
	return hr;
}

//=============================================================================

HRESULT CDeviceObjectFactory::InvalidateCpuCache(void* buffer_ptr, size_t size, size_t offset)
{
	return S_OK;
}

HRESULT CDeviceObjectFactory::InvalidateGpuCache(D3DBuffer* buffer, void* buffer_ptr, buffer_size_t size, buffer_size_t offset)
{
	return S_OK;
}

void CDeviceObjectFactory::InvalidateBuffer(D3DBuffer* buffer, void* base_ptr, buffer_size_t offset, buffer_size_t size, uint32 id)
{
}

//=============================================================================

uint8* CDeviceObjectFactory::Map(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	D3D11_MAPPED_SUBRESOURCE mapped_resource = { 0 };
#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
	HRESULT hr = DXGLMapBufferRange(
		gcpRendD3D->GetDeviceContext().GetRealDeviceContext()
		, buffer
		, offset
		, size
		, mode
		, 0
		, &mapped_resource);
	mapped_resource.pData = reinterpret_cast<uint8*>(mapped_resource.pData) - offset;
#else
	SIZE_T BeginEndR[2] = { offset, offset + size };
	HRESULT hr = gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Map(
		buffer
		, subresource
		, BeginEndR
		, mode
		, 0
		, &mapped_resource);
#endif
	return reinterpret_cast<uint8*>(mapped_resource.pData);
}

void CDeviceObjectFactory::Unmap(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	SIZE_T BeginEndW[2] = { offset, offset + size };
	gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Unmap(
		buffer
		, subresource
		, BeginEndW);
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::UploadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks)
{
	if (!bDirectAccess)
	{
		const uint8* pInData = reinterpret_cast<const uint8*>(pInDataCPU);
		uint8* pOutData = CDeviceObjectFactory::Map(buffer, subresource, offset, 0, mode) + offset;

		StreamBufferData(pOutData, pInData, size);

		CDeviceObjectFactory::Unmap(buffer, subresource, offset, size, mode);
	}
	else
	{
		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::MarkReadRange(buffer, offset, 0, D3D11_MAP_WRITE);

		StreamBufferData(pOutDataGPU, pInDataCPU, size);

		CDeviceObjectFactory::MarkWriteRange(buffer, offset, size, D3D11_MAP_WRITE);
	}
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::DownloadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks)
{
	if (!bDirectAccess)
	{
		uint8* pOutData = reinterpret_cast<uint8*>(pOutDataCPU);
		const uint8* pInData = CDeviceObjectFactory::Map(buffer, subresource, offset, size, mode) + offset;

		FetchBufferData(pOutData, pInData, size);

		CDeviceObjectFactory::Unmap(buffer, subresource, offset, 0, mode);
	}
	else
	{
		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::MarkReadRange(buffer, offset, size, D3D11_MAP_READ);

		FetchBufferData(pOutDataCPU, pInDataGPU, size);

		CDeviceObjectFactory::MarkWriteRange(buffer, offset, 0, D3D11_MAP_READ);
	}
}

// Explicit instantiation
template
void CDeviceObjectFactory::UploadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::UploadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);

template
void CDeviceObjectFactory::DownloadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::DownloadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);

// Shader API
ID3D11VertexShader* CDeviceObjectFactory::CreateVertexShader(const void* pData, size_t bytes)
{
	ID3D11VertexShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateVertexShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11PixelShader* CDeviceObjectFactory::CreatePixelShader(const void* pData, size_t bytes)
{
	ID3D11PixelShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreatePixelShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11GeometryShader* CDeviceObjectFactory::CreateGeometryShader(const void* pData, size_t bytes)
{
	ID3D11GeometryShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateGeometryShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11HullShader* CDeviceObjectFactory::CreateHullShader(const void* pData, size_t bytes)
{
	ID3D11HullShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateHullShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11DomainShader* CDeviceObjectFactory::CreateDomainShader(const void* pData, size_t bytes)
{
	ID3D11DomainShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateDomainShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11ComputeShader* CDeviceObjectFactory::CreateComputeShader(const void* pData, size_t bytes)
{
	ID3D11ComputeShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateComputeShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

// Occlusion Query API
D3DOcclusionQuery* CDeviceObjectFactory::CreateOcclusionQuery()
{
	D3DOcclusionQuery* pResult;
	D3D11_QUERY_DESC desc;
	desc.Query = D3D11_QUERY_OCCLUSION;
	desc.MiscFlags = 0;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateQuery(&desc, &pResult)) ? pResult : nullptr;
}

bool CDeviceObjectFactory::GetOcclusionQueryResults(D3DOcclusionQuery* pQuery, uint64& samplesPassed)
{
	return gcpRendD3D->GetDeviceContext().GetData(pQuery, &samplesPassed, sizeof(uint64), 0) == S_OK;
}
