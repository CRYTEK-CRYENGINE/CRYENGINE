// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeviceWrapper12.h"
#include "DriverD3D.h"
#include "../../Common/ReverseDepth.h"

#if !defined(CRY_USE_DX12_NATIVE) && !defined(CRY_USE_GNM_RENDERER)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	#define DX11_COMMANDLIST_REDUNDANT_STATE_FILTERING

class CDeviceGraphicsPSO_DX11 : public CDeviceGraphicsPSO
{
public:
	CDeviceGraphicsPSO_DX11();

	virtual bool Init(const CDeviceGraphicsPSODesc& psoDesc) final;

	_smart_ptr<ID3D11RasterizerState>                 pRasterizerState;
	_smart_ptr<ID3D11BlendState>                      pBlendState;
	_smart_ptr<ID3D11DepthStencilState>               pDepthStencilState;
	_smart_ptr<ID3D11InputLayout>                     pInputLayout;

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

	m_NumSamplers.fill(0);
	m_NumSRVs.fill(0);
}

bool CDeviceGraphicsPSO_DX11::Init(const CDeviceGraphicsPSODesc& psoDesc)
{
	CD3D9Renderer* rd = gcpRendD3D;
	m_bValid = false;
	m_nUpdateCount++;

	pRasterizerState = NULL;
	pBlendState = NULL;
	pDepthStencilState = NULL;
	pInputLayout = NULL;

	m_NumSamplers.fill(0);
	m_NumSRVs.fill(0);

	auto hwShaders = SDeviceObjectHelpers::GetShaderInstanceInfo(psoDesc.m_pShader, psoDesc.m_technique, psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, psoDesc.m_bAllowTesselation);

	// validate shaders first
	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (hwShaders[shaderClass].pHwShader && (hwShaders[shaderClass].pHwShaderInstance == NULL || hwShaders[shaderClass].pDeviceShader == NULL))
			return false;

		m_pDeviceShaders[shaderClass] = hwShaders[shaderClass].pDeviceShader;

		// TODO: remove
		m_pHwShaders[shaderClass] = hwShaders[shaderClass].pHwShader;
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
		return false;

	pDepthStencilState = rd->m_StatesDP[depthStateIndex].pState;
	pRasterizerState = rd->m_StatesRS[rasterStateIndex].pState;
	pBlendState = rd->m_StatesBL[blendStateIndex].pState;

	// input layout
	{
		if (CHWShader_D3D::SHWSInstance* pVsInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(hwShaders[eHWSC_Vertex].pHwShaderInstance))
		{
	#if defined(FEATURE_PER_SHADER_INPUT_LAYOUT_CACHE)
			// Try to find InputLayout in the cache
			const uint32 cacheID = CD3D9Renderer::FX_GetInputLayoutCacheId(pVsInstance->m_VStreamMask_Decl, psoDesc.m_VertexFormat);
			pInputLayout = pVsInstance->GetCachedInputLayout(cacheID);
			if (!pInputLayout)
			{
	#endif
			uint8 streamMask = psoDesc.CombineVertexStreamMasks(uint8(pVsInstance->m_VStreamMask_Decl), psoDesc.m_ObjectStreamMask);

			const bool bMorph = false;
			const bool bInstanced = (streamMask & VSM_INSTANCED) != 0;
			SOnDemandD3DVertexDeclarationCache& declCache = rd->m_RP.m_D3DVertexDeclarationCache[streamMask >> 1][psoDesc.m_VertexFormat][bMorph || bInstanced];

			if (!declCache.m_pDeclaration)
			{
				SOnDemandD3DVertexDeclaration Decl;
				rd->EF_OnDemandVertexDeclaration(Decl, streamMask >> 1, psoDesc.m_VertexFormat, bMorph, bInstanced);

				if (!Decl.m_Declaration.empty())
				{
					auto hr = rd->GetDevice().CreateInputLayout(&Decl.m_Declaration[0], Decl.m_Declaration.Num(), pVsInstance->m_pShaderData, pVsInstance->m_nDataSize, &declCache.m_pDeclaration);
					if (!SUCCEEDED(hr))
						return false;
				}
			}

			pInputLayout = declCache.m_pDeclaration;

	#if defined(FEATURE_PER_SHADER_INPUT_LAYOUT_CACHE)
		}
		if (pInputLayout)
		{
			pVsInstance->SetCachedInputLayout(pInputLayout, cacheID);
		}
	#endif
		}

		if (!pInputLayout)
			return false;
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

	m_PrimitiveTopology = rd->FX_ConvertPrimitiveType(psoDesc.m_PrimitiveType);
	m_ShaderFlags_RT = psoDesc.m_ShaderFlags_RT;
	m_ShaderFlags_MD = psoDesc.m_ShaderFlags_MD;
	m_ShaderFlags_MDV = psoDesc.m_ShaderFlags_MDV;

	#if defined(ENABLE_PROFILING_CODE)
	m_PrimitiveTypeForProfiling = psoDesc.m_PrimitiveType;
	#endif

	m_bValid = true;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceComputePSO_DX11 : public CDeviceComputePSO
{
public:
	CDeviceComputePSO_DX11() {}
	virtual ~CDeviceComputePSO_DX11() final {}
	virtual bool Init(const CDeviceComputePSODesc& psoDesc) final { return false; }
};

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

	struct SCompiledBuffer
	{
		ID3D11ShaderResourceView* pSrv;
		int slot;
	};

	CDeviceResourceSet_DX11(CDeviceResourceSet::EFlags flags)
		: CDeviceResourceSet(flags)
	{}

	CDeviceResourceSet_DX11(const CDeviceResourceSet& other)
		: CDeviceResourceSet(other)
	{
		Clear();
	}

	virtual void Build() final;

	void         Clear();

	// set via reflection from shader
	std::array<std::array<ID3D11ShaderResourceView*, MAX_TMU>, eHWSC_Num> compiledSRVs;
	std::array<std::array<ID3D11SamplerState*, MAX_TMU>, eHWSC_Num> compiledSamplers;

	// set directly
	std::array<std::array<SCompiledConstantBuffer, eConstantBufferShaderSlot_Count>, eHWSC_Num> compiledCBs;
	std::array<uint8, eHWSC_Num> numCompiledCBs;

	std::array<std::array<SCompiledBuffer, ResourceSetBufferCount>, eHWSC_Num> compiledBuffers;
	std::array<uint8, eHWSC_Num> numCompiledBuffers;

	std::vector<_smart_ptr<ID3D11ShaderResourceView>> m_SRVInUse;
	std::vector<_smart_ptr<ID3D11SamplerState>> m_SamplersInUse;
	std::vector<_smart_ptr<D3DBuffer>> m_CBInUse;

};

const void* const CDeviceResourceSet_DX11::InvalidPointer = (const void* const)0xBADA55;

void CDeviceResourceSet_DX11::Clear()
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	m_SRVInUse.clear();
	m_SamplersInUse.clear();
	m_CBInUse.clear();

	for (auto& stage : compiledSRVs)
		stage.fill((ID3D11ShaderResourceView*)InvalidPointer);

	for (auto& stage : compiledSamplers)
		stage.fill((ID3D11SamplerState*)InvalidPointer);

	SCompiledConstantBuffer nullCB;
	ZeroStruct(nullCB);

	for (auto& stage : compiledCBs)
		stage.fill(nullCB);

	SCompiledBuffer nullBuffer;
	ZeroStruct(nullBuffer);

	for (auto& stage : compiledBuffers)
		stage.fill(nullBuffer);

	numCompiledCBs.fill(0);
	numCompiledBuffers.fill(0);
}

void CDeviceResourceSet_DX11::Build()
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());
	CRY_ASSERT(m_Buffers.size() <= ResourceSetBufferCount);

	Clear();
	m_bValid = true;

	for (const auto& it : m_Textures)
	{
		ID3D11ShaderResourceView* pSrv = nullptr;

		if (CTexture* pTex = std::get<1>(it.second.resource))
		{
			if (pTex->GetDevTexture())
			{
				SResourceView::KeyType srvKey = std::get<0>(it.second.resource);
				pSrv = reinterpret_cast<ID3D11ShaderResourceView*>(pTex->GetShaderResourceView(srvKey));

				m_SRVInUse.push_back(pSrv);
			}
		}

		for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
		{
			if (it.second.shaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
				compiledSRVs[shaderClass][it.first] = pSrv;
		}

		m_bValid &= pSrv != nullptr;
	}

	for (const auto& it : m_Buffers)
	{
		SCompiledBuffer compiledBuffer;
		compiledBuffer.pSrv = it.second.resource.GetSRV();
		compiledBuffer.slot = it.first;

		for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
		{
			if (it.second.shaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
			{
				CRY_ASSERT(numCompiledBuffers[shaderClass] < ResourceSetBufferCount);
				compiledBuffers[shaderClass][numCompiledBuffers[shaderClass]++] = compiledBuffer;
			}
		}

		m_bValid &= (compiledBuffer.pSrv != nullptr) || (it.second.resource.m_flags & DX11BUF_NULL_RESOURCE); // If an intentional null resource was passed, the ResourceSet remains valid.
	}

	for (const auto& it : m_Samplers)
	{
		ID3D11SamplerState* pSamplerState = nullptr;

		if (it.second.resource >= 0 && it.second.resource < CTexture::s_TexStates.size())
		{
			const auto& ts = CTexture::s_TexStates[it.second.resource];
			pSamplerState = reinterpret_cast<ID3D11SamplerState*>(ts.m_pDeviceState);

			m_SamplersInUse.push_back(pSamplerState);
		}

		for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
		{
			if (it.second.shaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
				compiledSamplers[shaderClass][it.first] = pSamplerState;
		}

		m_bValid &= pSamplerState != nullptr;
	}

	for (const auto& it : m_ConstantBuffers)
	{
		SCompiledConstantBuffer compiledCB;
		ZeroStruct(compiledCB);

		if (it.second.resource)
		{
			size_t offset, size;
			compiledCB.pBuffer = it.second.resource->GetD3D(&offset, &size);
			compiledCB.code = it.second.resource->GetCode();
			compiledCB.offset = offset;
			compiledCB.size = size;
			compiledCB.slot = it.first;
		}

		for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
		{
			if (it.second.shaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
				compiledCBs[shaderClass][numCompiledCBs[shaderClass]++] = compiledCB;

			m_CBInUse.push_back(compiledCB.pBuffer);
		}

		m_bValid &= (compiledCB.pBuffer != nullptr) || (it.second.resource && it.second.resource->m_intentionallyNull); // If an intentional null resource was passed, the ResourceSet remains valid.
	}

	m_bDirty = !m_bValid;
}

class CDeviceResourceLayout_DX11 : public CDeviceResourceLayout
{
public:
	CDeviceResourceLayout_DX11(UsedBindSlotSet requiredResourceBindings)
		: CDeviceResourceLayout(requiredResourceBindings)
	{}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDeviceGraphicsCommandList_DX11 : public CDeviceGraphicsCommandList
{
public:
	CDeviceGraphicsCommandList_DX11()
	{
		Reset();
	}

	void SetResources_RequestedByShaderOnly(const CDeviceResourceSet_DX11* pResources);
	void SetResources_All(const CDeviceResourceSet_DX11* pResources);

	SCachedValue<ID3D11DepthStencilState*> m_CurrentDS;
	SCachedValue<ID3D11RasterizerState*> m_CurrentRS;
	SCachedValue<ID3D11BlendState*> m_CurrentBS;
	SCachedValue<ID3D11InputLayout*> m_CurrentInputLayout;
	SCachedValue<D3D11_PRIMITIVE_TOPOLOGY> m_CurrentTopology;
	SCachedValue<void*> m_CurrentShader[eHWSC_Num];
	SCachedValue<ID3D11ShaderResourceView*> m_CurrentSRV[eHWSC_Num][MAX_TMU];
	SCachedValue<ID3D11SamplerState*> m_CurrentSamplerState[eHWSC_Num][MAX_TMU];
	SCachedValue<uint64> m_CurrentCB[eHWSC_Num][eConstantBufferShaderSlot_Count];
	SCachedValue<const CDeviceInputStream*> m_CurrentVertexStreams;

	std::array<std::array<uint8, MAX_TMU>, eHWSC_Num> m_SRVs;
	std::array<std::array<uint8, MAX_TMU>, eHWSC_Num> m_Samplers;

	std::array<uint8, eHWSC_Num> m_NumSRVs;
	std::array<uint8, eHWSC_Num> m_NumSamplers;

	EShaderStage m_ValidShaderStages;

};

	#define GET_DX11_COMMANDLIST(abstractCommandList) reinterpret_cast<CDeviceGraphicsCommandList_DX11*>(abstractCommandList)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandList::PrepareRenderTargetsForUse(uint32 targetCount, CTexture** pTargets, SDepthTexture* pDepthTarget, SResourceView::KeyType* pRenderTargetViews) const
{
}

void CDeviceGraphicsCommandList::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
}

void CDeviceGraphicsCommandList::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
}

void CDeviceGraphicsCommandList::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages) const
{
}

void CDeviceGraphicsCommandList::PrepareVertexBuffersForUse(uint32 streamCount, const CDeviceInputStream* vertexStreams) const
{
}

void CDeviceGraphicsCommandList::PrepareIndexBufferForUse(const CDeviceInputStream* indexStream) const
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandList::SetRenderTargets(uint32 targetCount, CTexture* const* pTargets, const SDepthTexture* pDepthTarget, SResourceView::KeyType* pRenderTargetViews)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	ID3D11RenderTargetView* pRTV[CD3D9Renderer::RT_STACK_WIDTH] = { NULL };

	for (int i = 0; i < targetCount; ++i)
	{
		SResourceView::KeyType rtvKey = SResourceView::RenderTargetView(pTargets[i]->GetDstFormat()).m_Desc.Key;
		if (pRenderTargetViews && pRenderTargetViews[i] != SResourceView::DefaultRendertargtView)
			rtvKey = pRenderTargetViews[i];

		CRY_ASSERT(SResourceView(rtvKey).m_Desc.eViewType == SResourceView::eRenderTargetView);

		if (auto pView = reinterpret_cast<ID3D11RenderTargetView*>(pTargets[i]->GetResourceView(rtvKey).m_pDeviceResourceView))
			pRTV[i] = pView;
	}

	rd->GetDeviceContext().OMSetRenderTargets(targetCount, pRTV, pDepthTarget ? pDepthTarget->pSurface : NULL);
}

void CDeviceGraphicsCommandList::SetViewports(uint32 vpCount, const D3DViewPort* pViewports)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().RSSetViewports(vpCount, pViewports);
}

void CDeviceGraphicsCommandList::SetScissorRects(uint32 rcCount, const D3DRectangle* pRects)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().RSSetScissorRects(rcCount, pRects);
}

void CDeviceGraphicsCommandList::SetPipelineStateImpl(const CDeviceGraphicsPSOConstPtr& devicePSO)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const CDeviceGraphicsPSO_DX11* pDevicePSO = reinterpret_cast<const CDeviceGraphicsPSO_DX11*>(devicePSO.get());
	CDeviceGraphicsCommandList_DX11* pCmdList = GET_DX11_COMMANDLIST(this);

	// RasterState, DepthStencilState, BlendState
	if (pCmdList->m_CurrentDS.Set(pDevicePSO->pDepthStencilState))              rd->m_DevMan.SetDepthStencilState(pDevicePSO->pDepthStencilState, 0);
	if (pCmdList->m_CurrentBS.Set(pDevicePSO->pBlendState))                     rd->m_DevMan.SetBlendState(pDevicePSO->pBlendState, NULL, 0xffffffff);
	if (pCmdList->m_CurrentRS.Set(pDevicePSO->pRasterizerState))                rd->m_DevMan.SetRasterState(pDevicePSO->pRasterizerState);

	// Shaders
	const std::array<void*, eHWSC_Num>& shaders = pDevicePSO->m_pDeviceShaders;
	if (pCmdList->m_CurrentShader[eHWSC_Vertex].Set(shaders[eHWSC_Vertex]))     rd->m_DevMan.BindShader(CDeviceManager::TYPE_VS, (ID3D11Resource*)shaders[eHWSC_Vertex]);
	if (pCmdList->m_CurrentShader[eHWSC_Pixel].Set(shaders[eHWSC_Pixel]))       rd->m_DevMan.BindShader(CDeviceManager::TYPE_PS, (ID3D11Resource*)shaders[eHWSC_Pixel]);
	if (pCmdList->m_CurrentShader[eHWSC_Geometry].Set(shaders[eHWSC_Geometry])) rd->m_DevMan.BindShader(CDeviceManager::TYPE_GS, (ID3D11Resource*)shaders[eHWSC_Geometry]);
	if (pCmdList->m_CurrentShader[eHWSC_Domain].Set(shaders[eHWSC_Domain]))     rd->m_DevMan.BindShader(CDeviceManager::TYPE_DS, (ID3D11Resource*)shaders[eHWSC_Domain]);
	if (pCmdList->m_CurrentShader[eHWSC_Hull].Set(shaders[eHWSC_Hull]))         rd->m_DevMan.BindShader(CDeviceManager::TYPE_HS, (ID3D11Resource*)shaders[eHWSC_Hull]);

	// input layout and topology
	if (pCmdList->m_CurrentInputLayout.Set(pDevicePSO->pInputLayout))           rd->m_DevMan.BindVtxDecl(pDevicePSO->pInputLayout);
	if (pCmdList->m_CurrentTopology.Set(pDevicePSO->m_PrimitiveTopology))       rd->m_DevMan.BindTopology(pDevicePSO->m_PrimitiveTopology);

	// update valid shader mask
	pCmdList->m_ValidShaderStages = EShaderStage_None;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Vertex])   pCmdList->m_ValidShaderStages |= EShaderStage_Vertex;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Pixel])    pCmdList->m_ValidShaderStages |= EShaderStage_Pixel;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Geometry]) pCmdList->m_ValidShaderStages |= EShaderStage_Geometry;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Domain])   pCmdList->m_ValidShaderStages |= EShaderStage_Domain;
	if (pDevicePSO->m_pDeviceShaders[eHWSC_Hull])     pCmdList->m_ValidShaderStages |= EShaderStage_Hull;

	pCmdList->m_SRVs = pDevicePSO->m_SRVs;
	pCmdList->m_Samplers = pDevicePSO->m_Samplers;

	pCmdList->m_NumSRVs = pDevicePSO->m_NumSRVs;
	pCmdList->m_NumSamplers = pDevicePSO->m_NumSamplers;

	// TheoM TODO: REMOVE once shaders are set up completely via pso
	rd->m_RP.m_FlagsShader_RT = pDevicePSO->m_ShaderFlags_RT;
	rd->m_RP.m_FlagsShader_MD = pDevicePSO->m_ShaderFlags_MD;
	rd->m_RP.m_FlagsShader_MDV = pDevicePSO->m_ShaderFlags_MDV;
}

void CDeviceGraphicsCommandList::SetResourceLayoutImpl(const CDeviceResourceLayoutConstPtr& resourceLayout)
{
}

void CDeviceGraphicsCommandList::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	auto pCmdList = GET_DX11_COMMANDLIST(this);
	auto pResourcesDX11 = reinterpret_cast<const CDeviceResourceSet_DX11*>(pResources);

	if (pResources->GetFlags() & CDeviceResourceSet::EFlags_ForceSetAllState)
	{
		pCmdList->SetResources_All(pResourcesDX11);
	}
	else
	{
		pCmdList->SetResources_RequestedByShaderOnly(pResourcesDX11);
	}
}

// *INDENT-OFF*
void CDeviceGraphicsCommandList_DX11::SetResources_RequestedByShaderOnly(const CDeviceResourceSet_DX11* pResources)
{
	CD3D9Renderer *const __restrict rd = gcpRendD3D;

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (m_ValidShaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
		{
			// Bind SRVs
			// if (!pResources->compiledSRVs.empty()) // currently this is always the case
			{
				for (int i=0; i<m_NumSRVs[shaderClass]; ++i)
				{
					uint8 srvSlot = m_SRVs[shaderClass][i];
					ID3D11ShaderResourceView* pSrv = pResources->compiledSRVs[shaderClass][srvSlot];

					if (pSrv != CDeviceResourceSet_DX11::InvalidPointer)
					{
						if (m_CurrentSRV[shaderClass][srvSlot].Set(pSrv))
						{
							switch (shaderClass)
							{
							case eHWSC_Vertex:   rd->m_DevMan.BindSRV(CDeviceManager::TYPE_VS, pSrv, srvSlot); break;
							case eHWSC_Pixel:    rd->m_DevMan.BindSRV(CDeviceManager::TYPE_PS, pSrv, srvSlot); break;
							case eHWSC_Geometry: rd->m_DevMan.BindSRV(CDeviceManager::TYPE_GS, pSrv, srvSlot); break;
							case eHWSC_Domain:   rd->m_DevMan.BindSRV(CDeviceManager::TYPE_DS, pSrv, srvSlot); break;
							case eHWSC_Hull:     rd->m_DevMan.BindSRV(CDeviceManager::TYPE_HS, pSrv, srvSlot); break;
							default:
								CRY_ASSERT(false);
							}
						}
					}
				}
			}

			// Bind Samplers
			if (!pResources->compiledSamplers.empty())
			{
				for (int i=0; i<m_NumSamplers[shaderClass]; ++i)
				{
					uint8 samplerSlot = m_Samplers[shaderClass][i];
					ID3D11SamplerState* pSamplerState = pResources->compiledSamplers[shaderClass][samplerSlot];

					if (pSamplerState != CDeviceResourceSet_DX11::InvalidPointer)
					{
						if (m_CurrentSamplerState[shaderClass][samplerSlot].Set(pSamplerState))
						{
							switch (shaderClass)
							{
							case eHWSC_Vertex:   rd->m_DevMan.BindSampler(CDeviceManager::TYPE_VS, pSamplerState, samplerSlot); break;
							case eHWSC_Pixel:    rd->m_DevMan.BindSampler(CDeviceManager::TYPE_PS, pSamplerState, samplerSlot); break;
							case eHWSC_Geometry: rd->m_DevMan.BindSampler(CDeviceManager::TYPE_GS, pSamplerState, samplerSlot); break;
							case eHWSC_Domain:   rd->m_DevMan.BindSampler(CDeviceManager::TYPE_DS, pSamplerState, samplerSlot); break;
							case eHWSC_Hull:     rd->m_DevMan.BindSampler(CDeviceManager::TYPE_HS, pSamplerState, samplerSlot); break;
							default:
								CRY_ASSERT(false);
							}
						}
					}
				}
			}

			// Bind buffers
			for (int i=0; i<pResources->numCompiledBuffers[shaderClass]; ++i)
			{
				const CDeviceResourceSet_DX11::SCompiledBuffer& buffer = pResources->compiledBuffers[shaderClass][i];
				switch (shaderClass)
				{
				case eHWSC_Vertex:   rd->m_DevMan.BindSRV(CDeviceManager::TYPE_VS, buffer.pSrv, buffer.slot); break;
				case eHWSC_Pixel:    rd->m_DevMan.BindSRV(CDeviceManager::TYPE_PS, buffer.pSrv, buffer.slot); break;
				case eHWSC_Geometry: rd->m_DevMan.BindSRV(CDeviceManager::TYPE_GS, buffer.pSrv, buffer.slot); break;
				case eHWSC_Domain:   rd->m_DevMan.BindSRV(CDeviceManager::TYPE_DS, buffer.pSrv, buffer.slot); break;
				case eHWSC_Hull:     rd->m_DevMan.BindSRV(CDeviceManager::TYPE_HS, buffer.pSrv, buffer.slot); break;
				default:
					CRY_ASSERT(false);
				}
			}

			// Bind constant buffers
			for (int i=0; i<pResources->numCompiledCBs[shaderClass]; ++i)
			{
				const CDeviceResourceSet_DX11::SCompiledConstantBuffer& cb = pResources->compiledCBs[shaderClass][i];
				if (m_CurrentCB[shaderClass][cb.slot].Set(cb.code))
				{
					switch (shaderClass)
					{
					case eHWSC_Vertex:   rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_VS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
					case eHWSC_Pixel:    rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_PS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
					case eHWSC_Geometry: rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_GS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
					case eHWSC_Domain:   rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_DS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
					case eHWSC_Hull:     rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_HS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
					default:
						CRY_ASSERT(false);
					}
				}
			}
		}
	}
}

void CDeviceGraphicsCommandList_DX11::SetResources_All(const CDeviceResourceSet_DX11* pResources)
{
	CD3D9Renderer *const __restrict rd = gcpRendD3D;

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		// Bind SRVs
		for (int slot=0; slot<pResources->compiledSRVs[shaderClass].size(); ++slot)
		{
			ID3D11ShaderResourceView* pSrv = pResources->compiledSRVs[shaderClass][slot];

			if (pSrv != CDeviceResourceSet_DX11::InvalidPointer)
			{
				if (m_CurrentSRV[shaderClass][slot].Set(pSrv))
				{
					switch (shaderClass)
					{
					case eHWSC_Vertex:   rd->m_DevMan.BindSRV(CDeviceManager::TYPE_VS, pSrv, slot); break;
					case eHWSC_Pixel:    rd->m_DevMan.BindSRV(CDeviceManager::TYPE_PS, pSrv, slot); break;
					case eHWSC_Geometry: rd->m_DevMan.BindSRV(CDeviceManager::TYPE_GS, pSrv, slot); break;
					case eHWSC_Domain:   rd->m_DevMan.BindSRV(CDeviceManager::TYPE_DS, pSrv, slot); break;
					case eHWSC_Hull:     rd->m_DevMan.BindSRV(CDeviceManager::TYPE_HS, pSrv, slot); break;
					default:
						CRY_ASSERT(false);
					}
				}
			}
		}

		// Bind Samplers
		for (int slot=0; slot<pResources->compiledSamplers[shaderClass].size(); ++slot)
		{
			ID3D11SamplerState* pSamplerState = pResources->compiledSamplers[shaderClass][slot];

			if (pSamplerState != CDeviceResourceSet_DX11::InvalidPointer)
			{
				if (m_CurrentSamplerState[shaderClass][slot].Set(pSamplerState))
				{
					switch (shaderClass)
					{
					case eHWSC_Vertex:   rd->m_DevMan.BindSampler(CDeviceManager::TYPE_VS, pSamplerState, slot); break;
					case eHWSC_Pixel:    rd->m_DevMan.BindSampler(CDeviceManager::TYPE_PS, pSamplerState, slot); break;
					case eHWSC_Geometry: rd->m_DevMan.BindSampler(CDeviceManager::TYPE_GS, pSamplerState, slot); break;
					case eHWSC_Domain:   rd->m_DevMan.BindSampler(CDeviceManager::TYPE_DS, pSamplerState, slot); break;
					case eHWSC_Hull:     rd->m_DevMan.BindSampler(CDeviceManager::TYPE_HS, pSamplerState, slot); break;
					default:
						CRY_ASSERT(false);
					}
				}
			}
		}

		// Bind buffers
		for (int i=0; i<pResources->numCompiledBuffers[shaderClass]; ++i)
		{
			const CDeviceResourceSet_DX11::SCompiledBuffer& buffer = pResources->compiledBuffers[shaderClass][i];
			switch (shaderClass)
			{
			case eHWSC_Vertex:   rd->m_DevMan.BindSRV(CDeviceManager::TYPE_VS, buffer.pSrv, buffer.slot); break;
			case eHWSC_Pixel:    rd->m_DevMan.BindSRV(CDeviceManager::TYPE_PS, buffer.pSrv, buffer.slot); break;
			case eHWSC_Geometry: rd->m_DevMan.BindSRV(CDeviceManager::TYPE_GS, buffer.pSrv, buffer.slot); break;
			case eHWSC_Domain:   rd->m_DevMan.BindSRV(CDeviceManager::TYPE_DS, buffer.pSrv, buffer.slot); break;
			case eHWSC_Hull:     rd->m_DevMan.BindSRV(CDeviceManager::TYPE_HS, buffer.pSrv, buffer.slot); break;
			default:
				CRY_ASSERT(false);
			}
		}

		// Bind constant buffers
		for (int i=0; i<pResources->numCompiledCBs[shaderClass]; ++i)
		{
			const CDeviceResourceSet_DX11::SCompiledConstantBuffer& cb = pResources->compiledCBs[shaderClass][i];
			if (m_CurrentCB[shaderClass][cb.slot].Set(cb.code))
			{
				switch (shaderClass)
				{
				case eHWSC_Vertex:   rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_VS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
				case eHWSC_Pixel:    rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_PS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
				case eHWSC_Geometry: rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_GS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
				case eHWSC_Domain:   rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_DS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
				case eHWSC_Hull:     rd->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_HS, cb.pBuffer, cb.slot, cb.offset, cb.size); break;
				default:
					CRY_ASSERT(false);
				}
			}
		}
	}
}
// *INDENT-ON*

void CDeviceGraphicsCommandList::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (shaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
		{
			SetInlineConstantBuffer(bindSlot, pBuffer, shaderSlot, shaderClass);
		}
	}
}

void CDeviceGraphicsCommandList::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	auto pCmdList = GET_DX11_COMMANDLIST(this);

	if (pCmdList->m_CurrentCB[shaderClass][shaderSlot].Set(pBuffer->GetCode()))
	{
		switch (shaderClass)
		{
		case eHWSC_Vertex:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_VS, pBuffer, shaderSlot);
			break;
		case eHWSC_Pixel:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_PS, pBuffer, shaderSlot);
			break;
		case eHWSC_Geometry:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_GS, pBuffer, shaderSlot);
			break;
		case eHWSC_Domain:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_DS, pBuffer, shaderSlot);
			break;
		case eHWSC_Hull:
			gcpRendD3D->m_DevMan.BindConstantBuffer(CDeviceManager::TYPE_HS, pBuffer, shaderSlot);
			break;
		}
	}
}

void CDeviceGraphicsCommandList::SetVertexBuffers(uint32 streamCount, const CDeviceInputStream* vertexStreams)
{
	auto pCmdList = GET_DX11_COMMANDLIST(this);

	if (vertexStreams && pCmdList->m_CurrentVertexStreams.Set(vertexStreams))
	{
		for (uint32 slot = 0; slot < streamCount; ++slot)
		{
			const CDeviceInputStream& current = vertexStreams[slot];

			if (current.hStream != ~0u)
			{
				size_t offset;
				D3DBuffer* buffer = gcpRendD3D.m_DevBufMan.GetD3D(current.hStream, &offset);

				gcpRendD3D.m_DevMan.BindVB(buffer, slot, offset, current.nStride);
			}
		}
	}
}

void CDeviceGraphicsCommandList::SetIndexBuffer(const CDeviceInputStream* indexStream)
{
	auto pCmdList = GET_DX11_COMMANDLIST(this);

	if (pCmdList->m_CurrentIndexStream.Set(indexStream))
	{
		const CDeviceInputStream& current = indexStream[0];

		if (current.hStream != ~0u)
		{
			size_t offset;
			D3DBuffer* buffer = gcpRendD3D.m_DevBufMan.GetD3D(current.hStream, &offset);

	#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
			gcpRendD3D.m_DevMan.BindIB(buffer, offset, DXGI_FORMAT_R16_UINT);
	#else
			gcpRendD3D.m_DevMan.BindIB(buffer, offset, (DXGI_FORMAT)current.nStride);
	#endif
		}
	}
}

void CDeviceGraphicsCommandList::SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
}

void CDeviceGraphicsCommandList::SetStencilRefImpl(uint8 stencilRefValue)
{
	auto pCmdList = GET_DX11_COMMANDLIST(this);

	ID3D11DepthStencilState* pDS = NULL;

	if (m_pCurrentPipelineState)
		pDS = reinterpret_cast<const CDeviceGraphicsPSO_DX11*>(m_pCurrentPipelineState.get())->pDepthStencilState;

	gcpRendD3D->m_DevMan.SetDepthStencilState(pDS, stencilRefValue);

}

void CDeviceGraphicsCommandList::DrawImpl(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	if (InstanceCount > 1)
	{
		rd->m_DevMan.DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}
	else
	{
		rd->m_DevMan.Draw(VertexCountPerInstance, StartVertexLocation);
	}
}

void CDeviceGraphicsCommandList::DrawIndexedImpl(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
	STATIC_CHECK(false, "NOT IMPLEMENTED");
	#endif

	if (InstanceCount > 1)
	{
		rd->m_DevMan.DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}
	else
	{
		rd->m_DevMan.DrawIndexed(IndexCountPerInstance, StartIndexLocation, BaseVertexLocation);
	}
}

void CDeviceGraphicsCommandList::ClearSurface(D3DSurface* pView, const FLOAT Color[4], UINT NumRects, const D3D11_RECT* pRect)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->FX_ClearTarget(pView, ColorF(Color[0], Color[1], Color[2], Color[3]), NumRects, pRect);
}

void CDeviceGraphicsCommandList::LockToThread()
{
	// ...
}

void CDeviceGraphicsCommandList::Build()
{
	// pContext->FinishCommandList(false, ID3D11CommandList)
}

void CDeviceGraphicsCommandList::ResetImpl()
{
	auto pCmdList = GET_DX11_COMMANDLIST(this);

	pCmdList->m_CurrentDS = nullptr;
	pCmdList->m_CurrentInputLayout = nullptr;
	pCmdList->m_CurrentRS = nullptr;
	pCmdList->m_CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	pCmdList->m_CurrentBS = nullptr;
	pCmdList->m_ValidShaderStages = EShaderStage_All;

	const uint8 InvalidShaderPointer = (uint8)(uint64)CDeviceResourceSet_DX11::InvalidPointer;
	CRY_ASSERT((InvalidShaderPointer & 0x1) != 0);

	memset(pCmdList->m_CurrentShader, InvalidShaderPointer, sizeof(pCmdList->m_CurrentShader));
	memset(pCmdList->m_CurrentSRV, InvalidShaderPointer, sizeof(pCmdList->m_CurrentSRV));
	memset(pCmdList->m_CurrentSamplerState, InvalidShaderPointer, sizeof(pCmdList->m_CurrentSamplerState));
	memset(pCmdList->m_CurrentCB, InvalidShaderPointer, sizeof(pCmdList->m_CurrentCB));

	pCmdList->m_CurrentIndexStream = nullptr;
	pCmdList->m_CurrentVertexStreams = nullptr;

	pCmdList->m_NumSRVs.fill(0);
	pCmdList->m_NumSamplers.fill(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDeviceComputeCommandList_DX12 : public CDeviceComputeCommandList
{
public:
	CDeviceComputeCommandList_DX12()
	{
		Reset();
	}
};

	#define GET_DX12_COMPUTE_COMMANDLIST(abstractCommandList)       reinterpret_cast<CDeviceComputeCommandList_DX12*>(abstractCommandList)
	#define GET_DX12_COMPUTE_COMMANDLIST_CONST(abstractCommandList) reinterpret_cast<const CDeviceComputeCommandList_DX12*>(abstractCommandList)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceComputeCommandList::PrepareUATargetsForUse(uint32 targetCount, CGpuBuffer** pTargets) const
{
	__debugbreak();
}

void CDeviceComputeCommandList::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResourceSet) const
{
	__debugbreak();
}

void CDeviceComputeCommandList::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pConstantBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
	__debugbreak();
}

void CDeviceComputeCommandList::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages) const
{
	__debugbreak();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceComputeCommandList::SetUATargets(uint32 targetCount, const CGpuBuffer* const* pTargets)
{
	__debugbreak();
}

void CDeviceComputeCommandList::SetPipelineStateImpl(const CDeviceComputePSOConstPtr& devicePSO)
{
	__debugbreak();
}

void CDeviceComputeCommandList::SetResourceLayoutImpl(const CDeviceResourceLayoutConstPtr& resourceLayout)
{
	__debugbreak();
}

void CDeviceComputeCommandList::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResourceSet)
{
	__debugbreak();
}

void CDeviceComputeCommandList::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages)
{
	__debugbreak();
}

void CDeviceComputeCommandList::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pConstantBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	__debugbreak();
}

void CDeviceComputeCommandList::SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
	__debugbreak();
}

void CDeviceComputeCommandList::DispatchImpl(uint32 X, uint32 Y, uint32 Z)
{
	__debugbreak();
}

void CDeviceComputeCommandList::ClearUAV(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRect)
{
	__debugbreak();
}

void CDeviceComputeCommandList::ClearUAV(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRect)
{
	__debugbreak();
}

void CDeviceComputeCommandList::LockToThread()
{
	// ...
	__debugbreak();
}

void CDeviceComputeCommandList::Build()
{
	__debugbreak();
	// pContext->FinishCommandList(false, ID3D11CommandList)
}

void CDeviceComputeCommandList::ResetImpl()
{
	__debugbreak();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeviceCopyCommandList::LockToThread()
{
	// ...
}

void CDeviceCopyCommandList::Build()
{
	// pContext->FinishCommandList(false, ID3D11CommandList)
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceObjectFactory::CDeviceObjectFactory()
{
	m_pCoreCommandList = std::make_shared<CDeviceGraphicsCommandList_DX11>();
}

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

CDeviceResourceSetPtr CDeviceObjectFactory::CloneResourceSet(const CDeviceResourceSetPtr pSrcResourceSet) const
{
	return std::make_shared<CDeviceResourceSet_DX11>(*pSrcResourceSet);
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const
{
	return std::make_shared<CDeviceResourceLayout_DX11>(resourceLayoutDesc.GetRequiredResourceBindings());
}

CDeviceGraphicsCommandListPtr CDeviceObjectFactory::GetCoreGraphicsCommandList() const
{
	return m_pCoreCommandList;
}

// Acquire one or more command-lists which are independent of the core command-list
// Only one thread is allowed to call functions on this command-list (DX12 restriction).
// The thread that gets the permition is the one calling Begin() on it AFAIS
CDeviceGraphicsCommandListUPtr CDeviceObjectFactory::AcquireGraphicsCommandList()
{
	// TODO: implement deferred contexts
	__debugbreak();

	return CryMakeUnique<CDeviceGraphicsCommandList>();
}

std::vector<CDeviceGraphicsCommandListUPtr> CDeviceObjectFactory::AcquireGraphicsCommandLists(uint32 listCount)
{
	// TODO: implement deferred contexts
	__debugbreak();

	return std::move(std::vector<CDeviceGraphicsCommandListUPtr>(size_t(listCount)));
}

void CDeviceObjectFactory::ForfeitGraphicsCommandList(CDeviceGraphicsCommandListUPtr pCommandList)
{
	// TODO: implement deferred contexts
	__debugbreak();

	// pContext->ExecuteCommandList(ID3D11CommandList)
}

void CDeviceObjectFactory::ForfeitGraphicsCommandLists(std::vector<CDeviceGraphicsCommandListUPtr> pCommandLists)
{
	// TODO: implement deferred contexts
	__debugbreak();

	// pContext->ExecuteCommandList(ID3D11CommandList)
}

void CDeviceObjectFactory::ReleaseResourcesImpl()
{

}

#endif
