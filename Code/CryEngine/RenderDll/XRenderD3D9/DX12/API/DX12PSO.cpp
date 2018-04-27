// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12PSO.hpp"
#include "DX12Device.hpp"

namespace NCryDX12
{

bool CPSO::Init(ID3D12PipelineState* pD3D12PipelineState)
{
	m_pD3D12PipelineState = pD3D12PipelineState;

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
bool CGraphicsPSO::Init(const SInitParams& params)
{
	m_Desc = params.m_Desc;

	ID3D12PipelineState* pipelineState12 = NULL;
	HRESULT result = GetDevice()->GetD3D12Device()->CreateGraphicsPipelineState(&m_Desc, IID_GFX_ARGS(&pipelineState12));

	if (result != S_OK)
	{
		DX12_ERROR("Could not create graphics pipeline state!");
		return false;
	}

	CPSO::Init(pipelineState12);
	pipelineState12->Release();

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
bool CComputePSO::Init(const SInitParams& params)
{
	m_Desc = params.m_Desc;

	ID3D12PipelineState* pipelineState12 = NULL;
	HRESULT result = GetDevice()->GetD3D12Device()->CreateComputePipelineState(&m_Desc, IID_GFX_ARGS(&pipelineState12));

	if (result != S_OK)
	{
		DX12_ERROR("Could not create graphics pipeline state!");
		return false;
	}

	CPSO::Init(pipelineState12);
	pipelineState12->Release();

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
CPSOCache::CPSOCache()
	: m_pDevice(NULL)
{

}

//---------------------------------------------------------------------------------------------------------------------
CPSOCache::~CPSOCache()
{

}

//---------------------------------------------------------------------------------------------------------------------
bool CPSOCache::Init(CDevice* device)
{
	m_pDevice = device;
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
void CPSOCache::GetOrCreatePSO(const CGraphicsPSO::SInitParams& params, DX12_PTR(CGraphicsPSO)& result)
{
#ifdef DX12_PRECISE_DEDUPLICATION
	// NOTE: Hash the contents of the shaders instead of the pointer (which can change)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = params.m_Desc;

	if (params.m_VS) *((UINT64*)&Desc.VS.pShaderBytecode) = params.m_VS->GetHash();
	if (params.m_HS) *((UINT64*)&Desc.HS.pShaderBytecode) = params.m_HS->GetHash();
	if (params.m_DS) *((UINT64*)&Desc.DS.pShaderBytecode) = params.m_DS->GetHash();
	if (params.m_GS) *((UINT64*)&Desc.GS.pShaderBytecode) = params.m_GS->GetHash();
	if (params.m_PS) *((UINT64*)&Desc.PS.pShaderBytecode) = params.m_PS->GetHash();
#else
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc = params.m_Desc;
#endif

	// LSB cleared marks graphics pipeline states, in case graphics and compute hashes collide
	THash hash = (~1) & ComputeSmallHash<sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC)>(&Desc);
	TPSOMap::iterator iter = m_PSOMap.find(hash);

	if (iter != m_PSOMap.end())
	{
		result = static_cast<CGraphicsPSO*>(iter->second.get());

#if defined(DEBUG)
		D3D12_GRAPHICS_PIPELINE_STATE_DESC DescC = result.get()->GetDesc();

	#ifdef DX12_PRECISE_DEDUPLICATION
		if (params.m_VS) *((UINT64*)&DescC.VS.pShaderBytecode) = params.m_VS->GetHash();
		if (params.m_HS) *((UINT64*)&DescC.HS.pShaderBytecode) = params.m_HS->GetHash();
		if (params.m_DS) *((UINT64*)&DescC.DS.pShaderBytecode) = params.m_DS->GetHash();
		if (params.m_GS) *((UINT64*)&DescC.GS.pShaderBytecode) = params.m_GS->GetHash();
		if (params.m_PS) *((UINT64*)&DescC.PS.pShaderBytecode) = params.m_PS->GetHash();
	#endif

		DX12_ASSERT(!memcmp(&DescC, &Desc, sizeof(Desc)), "Hash collision!");
#endif
		return;
	}

	result = new CGraphicsPSO(m_pDevice);
	if (!result->Init(params))
	{
		DX12_ERROR("Could not create PSO!");
		result = NULL;
		return;
	}

	m_PSOMap[hash] = result;
}

//---------------------------------------------------------------------------------------------------------------------
void CPSOCache::GetOrCreatePSO(const CComputePSO::SInitParams& params, DX12_PTR(CComputePSO)& result)
{
#ifdef DX12_PRECISE_DEDUPLICATION
	// NOTE: Hash the contents of the shaders instead of the pointer (which can change)
	D3D12_COMPUTE_PIPELINE_STATE_DESC Desc = params.m_Desc;

	if (params.m_CS) *((UINT64*)&Desc.CS.pShaderBytecode) = params.m_CS->GetHash();
#else
	const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc = params.m_Desc;
#endif

	// LSB filled marks compute pipeline states, in case graphics and compute hashes collide
	THash hash = (1) | ComputeSmallHash<sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC)>(&Desc);
	TPSOMap::iterator iter = m_PSOMap.find(hash);

	if (iter != m_PSOMap.end())
	{
		result = static_cast<CComputePSO*>(iter->second.get());

#if defined(DEBUG)
		D3D12_COMPUTE_PIPELINE_STATE_DESC DescC = result.get()->GetDesc();

	#ifdef DX12_PRECISE_DEDUPLICATION
		if (params.m_CS) *((UINT64*)&DescC.CS.pShaderBytecode) = params.m_CS->GetHash();
	#endif

		DX12_ASSERT(!memcmp(&DescC, &Desc, sizeof(Desc)), "Hash collision!");
#endif
		return;
	}

	result = new CComputePSO(m_pDevice);
	if (!result->Init(params))
	{
		DX12_ERROR("Could not create PSO!");
		result = NULL;
		return;
	}

	m_PSOMap[hash] = result;
}

}
