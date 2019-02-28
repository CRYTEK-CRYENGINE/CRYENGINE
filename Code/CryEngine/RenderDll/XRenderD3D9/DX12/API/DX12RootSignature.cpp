// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12RootSignature.hpp"

#define DX12_DESCRIPTORTABLE_DETECT_VISIBILITY
#define DX12_DESCRIPTORTABLE_MERGERANGES_CBV  1
#define DX12_DESCRIPTORTABLE_MERGERANGES_SRV  2
#define DX12_DESCRIPTORTABLE_MERGERANGES_UAV  4
#define DX12_DESCRIPTORTABLE_MERGERANGES_SMP  8
#define DX12_DESCRIPTORTABLE_MERGERANGES_MODE (/*1+*/ 2 + 4 + 8)

namespace NCryDX12
{

//---------------------------------------------------------------------------------------------------------------------
void SResourceMappings::Init(CShader* vs, CShader* hs, CShader* ds, CShader* gs, CShader* ps)
{
	Clear();

	CShader* pShaderPipeline[6] = { vs, hs, ds, gs, ps, nullptr };
	for (EShaderStage eStage = ESS_First; eStage < ESS_Num; eStage = EShaderStage(eStage + 1))
	{
		CShader* pShader = pShaderPipeline[eStage];
		if (pShader)
		{
			Append(pShader, eStage);
		}
	}
}

void SResourceMappings::Init(CShader* cs)
{
	Clear();

	CShader* pShaderPipeline[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, cs };
	for (EShaderStage eStage = ESS_First; eStage < ESS_Num; eStage = EShaderStage(eStage + 1))
	{
		CShader* pShader = pShaderPipeline[eStage];
		if (pShader)
		{
			Append(pShader, eStage);
		}
	}
}

void SResourceMappings::Clear()
{
	memset(&m_ConstantViews, ESS_None, sizeof(m_ConstantViews));
	m_NumConstantViews = 0;

	m_DescRangeCursor = 0;
	m_NumResources = 0;
	m_NumDynamicSamplers = 0;
	m_NumRootParameters = 0;
	m_NumConstantViews = 0;
	m_NumStaticSamplers = 0;
}

void SResourceMappings::Append(UINT currentRangeCursor, UINT& lastRangeCursor, UINT currentOffset, UINT& lastOffset, D3D12_DESCRIPTOR_HEAP_TYPE eHeap, D3D12_SHADER_VISIBILITY eVisibility)
{
	const UINT numRanges = currentRangeCursor - lastRangeCursor;
	if (numRanges > 0)
	{
		SResourceMappings::SDescriptorTableInfo tableInfo;
		tableInfo.m_Type = eHeap;
		tableInfo.m_Offset = lastOffset;
		m_DescriptorTables[m_NumDescriptorTables++] = tableInfo;

		// Setup the configuration of the root signature entry (a descriptor table range in this case)
		m_RootParameters[m_NumRootParameters++].InitAsDescriptorTable(numRanges, m_DescRanges + lastRangeCursor, eVisibility);

		lastRangeCursor = currentRangeCursor;
		lastOffset = currentOffset;

		DX12_ASSERT(m_NumDescriptorTables <= 64, "Descriptor table overflow!");
		DX12_ASSERT(m_NumRootParameters <= 64, "Root parameter overflow!");
	}
}

void SResourceMappings::Append(CShader* pShader, EShaderStage eStage)
{
	const D3D12_SHADER_VISIBILITY eVisibility = static_cast<D3D12_SHADER_VISIBILITY>((eStage + 1) % (D3D12_SHADER_VISIBILITY_PIXEL + 1));

	UINT lastRangeCursor = m_DescRangeCursor;
	UINT currentRangeCursor = m_DescRangeCursor;
	UINT currentConstantViews = m_NumConstantViews;

	{
		const SResourceRanges& rr = pShader->GetResourceRanges();
		UINT lastResources = m_NumResources;
		UINT currentResources = m_NumResources;

		if (rr.m_ConstantBuffers.m_TotalLength)
		{
			const SBindRanges::TRanges& ranges = rr.m_ConstantBuffers.m_Ranges;
			for (size_t ri = 0, S = ranges.size(); ri < S; ++ri)
			{
				const SBindRanges::SRange range = ranges[ri];

#if defined(DX12_DESCRIPTORTABLE_DETECT_VISIBILITY)
				if (range.m_bShared)
				{
					// only pick up the first occurrence
					if (m_ConstantViews[range.m_Start] != ESS_None)
					{
						continue;
					}

					m_ConstantViews[range.m_Start] = eStage;

					// Setup the configuration of the root signature entry (a constant static CBV in this case)
					for (UINT rj = range.m_Start, rn = range.m_Start + range.m_Length; rj < rn; ++rj)
					{
						m_RootParameters[m_NumRootParameters++].InitAsConstantBufferView(rj, 0, D3D12_SHADER_VISIBILITY_ALL);
						DX12_ASSERT(m_NumRootParameters <= 64, "Root parameter overflow!");
					}

					continue;
				}
#endif

				if (!range.m_bUsed)
				{
					// Setup the configuration of the root signature entry (a constant dummy CBV in this case)
					for (UINT rj = range.m_Start, rn = range.m_Start + range.m_Length; rj < rn; ++rj)
					{
						m_RootParameters[m_NumRootParameters++].InitAsConstantBufferView(rj, 0, eVisibility);
						DX12_ASSERT(m_NumRootParameters <= 64, "Root parameter overflow!");
					}

					continue;
				}

				if (!(DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_CBV) && (range.m_bUnmergable))
				{
					Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
				}

				// Setup the configuration of the descriptor table range (a number of CBVs in this case)
				m_DescRanges[currentRangeCursor++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, range.m_Length, range.m_Start);
				DX12_ASSERT(currentRangeCursor <= 128, "Range cursor overflow!");

				for (size_t mi = range.m_Start, SS = mi + range.m_Length; mi < SS; ++mi, ++currentResources)
				{
					ShaderInputMapping shaderInputMapping;
					ZeroStruct(shaderInputMapping);

					shaderInputMapping.ViewType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
					shaderInputMapping.ShaderStage = eStage;
					shaderInputMapping.ShaderSlot = uint8_t(mi);
#ifdef _DEBUG
					shaderInputMapping.DescriptorOffset = currentResources;
#endif
					m_RESs[m_NumRESs++] = shaderInputMapping;
					DX12_ASSERT(m_NumRESs <= 128, "Input mapping overflow!");
				}

				if (!(DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_CBV) || (range.m_bUnmergable))
				{
					Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
				}
			}
		}

		if ((DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_CBV))
		{
			Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
		}

		if (rr.m_InputResources.m_TotalLength)
		{
			const SBindRanges::TRanges& ranges = rr.m_InputResources.m_Ranges;
			for (size_t ri = 0, S = ranges.size(); ri < S; ++ri)
			{
				const SBindRanges::SRange range = ranges[ri];

				if (!range.m_bUsed)
				{
					// Setup the configuration of the root signature entry (a constant dummy SRV in this case)
					for (UINT rj = range.m_Start, rn = range.m_Start + range.m_Length; rj < rn; ++rj)
					{
						m_RootParameters[m_NumRootParameters++].InitAsShaderResourceView(rj, 0, eVisibility);
						DX12_ASSERT(m_NumRootParameters <= 64, "Root parameter overflow!");
					}

					continue;
				}

				if (!(DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_SRV) && (range.m_bUnmergable))
				{
					Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
				}

				// Setup the configuration of the descriptor table range (a number of SRVs in this case)
				m_DescRanges[currentRangeCursor++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, range.m_Length, range.m_Start);
				DX12_ASSERT(currentRangeCursor <= 128, "Range cursor overflow!");

				for (size_t mi = range.m_Start, j = 0, SS = mi + range.m_Length; mi < SS; ++mi, ++j, ++currentResources)
				{
					ShaderInputMapping shaderInputMapping;
					ZeroStruct(shaderInputMapping);

					shaderInputMapping.ViewType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					shaderInputMapping.ShaderStage = eStage;
					shaderInputMapping.ShaderSlot = uint8_t(mi);
#ifdef _DEBUG
					shaderInputMapping.DescriptorOffset = currentResources;
#endif
					m_RESs[m_NumRESs++] = shaderInputMapping;
					DX12_ASSERT(m_NumRESs <= 128, "Input mapping overflow!");
				}

				if (!(DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_SRV) || (range.m_bUnmergable))
				{
					Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
				}
			}
		}

		if ((DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_SRV))
		{
			Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
		}

		if (rr.m_OutputResources.m_TotalLength)
		{
			const SBindRanges::TRanges& ranges = rr.m_OutputResources.m_Ranges;
			for (size_t ri = 0, S = ranges.size(); ri < S; ++ri)
			{
				const SBindRanges::SRange range = ranges[ri];

				if (!range.m_bUsed)
				{
					// Setup the configuration of the root signature entry (a constant dummy UAV in this case)
					for (UINT rj = range.m_Start, rn = range.m_Start + range.m_Length; rj < rn; ++rj)
					{
						m_RootParameters[m_NumRootParameters++].InitAsUnorderedAccessView(rj, 0, eVisibility);
						DX12_ASSERT(m_NumRootParameters <= 64, "Root parameter overflow!");
					}

					continue;
				}

				if (!(DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_UAV) && (range.m_bUnmergable))
				{
					Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
				}

				// Setup the configuration of the descriptor table range (a number of UAVs in this case)
				m_DescRanges[currentRangeCursor++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, range.m_Length, range.m_Start);
				DX12_ASSERT(currentRangeCursor <= 128, "Range cursor overflow!");

				for (size_t mi = range.m_Start, j = 0, SS = mi + range.m_Length; mi < SS; ++mi, ++j, ++currentResources)
				{
					ShaderInputMapping shaderInputMapping;
					ZeroStruct(shaderInputMapping);

					shaderInputMapping.ViewType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					shaderInputMapping.ShaderStage = eStage;
					shaderInputMapping.ShaderSlot = uint8_t(mi);
#ifdef _DEBUG
					shaderInputMapping.DescriptorOffset = currentResources;
#endif
					m_RESs[m_NumRESs++] = shaderInputMapping;
					DX12_ASSERT(m_NumRESs <= 128, "Input mapping overflow!");
				}

				if (!(DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_UAV) || (range.m_bUnmergable))
				{
					Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
				}
			}
		}

		if ((DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_UAV))
		{
			Append(currentRangeCursor, lastRangeCursor, currentResources, lastResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, eVisibility);
		}

		m_NumResources = currentResources;
	}

	{
		const SResourceRanges& rr = pShader->GetResourceRanges();
		UINT lastSamplers = m_NumDynamicSamplers;
		UINT currentSamplers = m_NumDynamicSamplers;

		if (rr.m_Samplers.m_TotalLength)
		{
			const SBindRanges::TRanges& ranges = rr.m_Samplers.m_Ranges;
			for (size_t ri = 0, S = ranges.size(); ri < S; ++ri)
			{
				const SBindRanges::SRange range = ranges[ri];

				if (!range.m_bUsed)
				{
					// Setup the configuration of the root signature entry (a constant dummy UAV in this case)
					for (UINT rj = range.m_Start, rn = range.m_Start + range.m_Length; rj < rn; ++rj)
					{
						__debugbreak();
						//						m_StaticSamplers[m_NumStaticSamplers++].Init(rj, 0, ..., static_cast<D3D12_SHADER_VISIBILITY>((stage + 1) % (D3D12_SHADER_VISIBILITY_PIXEL + 1)));
					}

					continue;
				}

				if (!(DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_SMP) && (range.m_bUnmergable))
				{
					Append(currentRangeCursor, lastRangeCursor, currentSamplers, lastSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, eVisibility);
				}

				// Setup the configuration of the descriptor table range (a number of Samplers in this case)
				m_DescRanges[currentRangeCursor++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, range.m_Length, range.m_Start);
				DX12_ASSERT(currentRangeCursor <= 128, "Range cursor overflow!");

				for (size_t mi = range.m_Start, SS = mi + range.m_Length; mi < SS; ++mi, ++currentSamplers)
				{
					ShaderInputMapping shaderInputMapping;
					ZeroStruct(shaderInputMapping);

					shaderInputMapping.ViewType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
					shaderInputMapping.ShaderStage = eStage;
					shaderInputMapping.ShaderSlot = uint8_t(mi);
#ifdef _DEBUG
					shaderInputMapping.DescriptorOffset = currentSamplers;
#endif
					m_SMPs[m_NumSMPs++] = shaderInputMapping;
					DX12_ASSERT(m_NumSMPs <= 32, "Input mapping overflow!");
				}

				if (!(DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_SMP) || (range.m_bUnmergable))
				{
					Append(currentRangeCursor, lastRangeCursor, currentSamplers, lastSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, eVisibility);
				}
			}
		}

		if ((DX12_DESCRIPTORTABLE_MERGERANGES_MODE & DX12_DESCRIPTORTABLE_MERGERANGES_SMP))
		{
			Append(currentRangeCursor, lastRangeCursor, currentSamplers, lastSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, eVisibility);
		}

		m_NumDynamicSamplers = currentSamplers;
	}

	m_DescRangeCursor = currentRangeCursor;
	m_NumConstantViews = currentConstantViews;
}

//---------------------------------------------------------------------------------------------------------------------

void SResourceMappings::DebugPrint() const
{
	DX12_LOG(g_nPrintDX12, "Root Signature:");
	for (UINT p = 0, r = 0; p < m_NumRootParameters; ++p)
	{
		DX12_LOG(g_nPrintDX12, " %s: %c [%2d to %2d] -> %s [%2d to %2d]",
		         m_RootParameters[p].ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL ? "Every    shader stage" :
		         m_RootParameters[p].ShaderVisibility == D3D12_SHADER_VISIBILITY_VERTEX ? "Vertex   shader stage" :
		         m_RootParameters[p].ShaderVisibility == D3D12_SHADER_VISIBILITY_HULL ? "Hull     shader stage" :
		         m_RootParameters[p].ShaderVisibility == D3D12_SHADER_VISIBILITY_DOMAIN ? "Domain   shader stage" :
		         m_RootParameters[p].ShaderVisibility == D3D12_SHADER_VISIBILITY_GEOMETRY ? "Geometry shader stage" :
		         m_RootParameters[p].ShaderVisibility == D3D12_SHADER_VISIBILITY_PIXEL ? "Pixel    shader stage" : "Unknown  shader stage",
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && m_RootParameters[p].DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV ? 'C' :
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && m_RootParameters[p].DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ? 'T' :
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && m_RootParameters[p].DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV ? 'U' :
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && m_RootParameters[p].DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? 'S' :
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ? 'c' :
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ? 't' :
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV ? 'u' : '?',
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE ? m_RootParameters[p].DescriptorTable.pDescriptorRanges->BaseShaderRegister : m_RootParameters[p].Descriptor.ShaderRegister,
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE ? m_RootParameters[p].DescriptorTable.pDescriptorRanges->BaseShaderRegister + m_RootParameters[p].DescriptorTable.pDescriptorRanges->NumDescriptors : 1,
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE ? "descriptor table" :
		         m_RootParameters[p].ParameterType != D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS ? "descriptor" : "32bit constant",
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE ? m_DescriptorTables[r].m_Offset : -1,
		         m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE ? m_DescriptorTables[r].m_Offset + m_RootParameters[p].DescriptorTable.pDescriptorRanges->NumDescriptors : -1);

		r += m_RootParameters[p].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
CRootSignature::CRootSignature(CDevice* device)
	: CDeviceObject(device)
{

}

//---------------------------------------------------------------------------------------------------------------------
CRootSignature::~CRootSignature()
{

}

//---------------------------------------------------------------------------------------------------------------------
bool CRootSignature::Init(const SResourceMappings& resourceMappings, bool bGfx, UINT nodeMask)
{
	m_ResourceMappings = resourceMappings;

	return Serialize(bGfx, nodeMask);
}

bool CRootSignature::Serialize(bool bGfx, UINT nodeMask)
{
	ID3D12RootSignature* rootSign = NULL;
	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;

	if (bGfx)
	{
		descRootSignature.Init(m_ResourceMappings.m_NumRootParameters, m_ResourceMappings.m_RootParameters,
		                       m_ResourceMappings.m_NumStaticSamplers, m_ResourceMappings.m_StaticSamplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	}
	else
	{
		descRootSignature.Init(m_ResourceMappings.m_NumRootParameters, m_ResourceMappings.m_RootParameters,
		                       0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	}

	{
		ID3DBlob* pOutBlob = NULL;
		ID3DBlob* pErrorBlob = NULL;
		HRESULT result;

		result = D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob);

		if (result != S_OK)
		{
			if (pErrorBlob)
			{
				void* pError = pErrorBlob->GetBufferPointer();
				size_t sizeError = pErrorBlob->GetBufferSize();

				DX12_ERROR("Could not serialize root signature: %s", pErrorBlob->GetBufferPointer());

				pErrorBlob->Release();
			}
			else
			{
				DX12_ERROR("Could not serialize root signature!");
			}

			return false;
		}

		if (pErrorBlob)
		{
			pErrorBlob->Release();
		}

		result = GetDevice()->GetD3D12Device()->CreateRootSignature(m_nodeMask = nodeMask, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_GFX_ARGS(&rootSign));

		if (pOutBlob)
		{
			pOutBlob->Release();
		}

		if (result != S_OK)
		{
			DX12_ERROR("Could not create root signature!");
			return false;
		}
	}

	m_pRootSignature = rootSign;
	rootSign->Release();

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
CRootSignatureCache::CRootSignatureCache()
{

}

//---------------------------------------------------------------------------------------------------------------------
CRootSignatureCache::~CRootSignatureCache()
{

}

//---------------------------------------------------------------------------------------------------------------------
bool CRootSignatureCache::Init(CDevice* device)
{
	m_pDevice = device;
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
void CRootSignatureCache::GetOrCreateRootSignature(const CRootSignature::SInitParams& params, bool bGfx, UINT nodeMask, DX12_PTR(CRootSignature)& result)
{
#ifdef DX12_PRECISE_DEDUPLICATION
	// NOTE: Hash the contents of the shaders instead of the pointer (which can change)
	CRootSignature::SInitParams Parm = params;

	if (Parm.m_VS) *((UINT64*)&Parm.m_VS) = params.m_VS->GetHash();
	if (Parm.m_HS) *((UINT64*)&Parm.m_HS) = params.m_HS->GetHash();
	if (Parm.m_DS) *((UINT64*)&Parm.m_DS) = params.m_DS->GetHash();
	if (Parm.m_GS) *((UINT64*)&Parm.m_GS) = params.m_GS->GetHash();
	if (Parm.m_PS) *((UINT64*)&Parm.m_PS) = params.m_PS->GetHash();
#else
	const CRootSignature::SInitParams& Parm = params;
#endif

	// LSB filled marks compute pipeline states, in case graphics and compute hashes collide
	THash hash = (bGfx ? 1 : 0) | ((~1) & ComputeSmallHash<sizeof(CRootSignature::SInitParams)>(&Parm));
	TRootSignatureMap::iterator iter = m_RootSignatureMapL0.find(hash);

	if (iter != m_RootSignatureMapL0.end())
	{
		result = iter->second;

#if defined(DX12_PRECISE_DEDUPLICATION) && defined(DEBUG)
		CRootSignature::SInitParams ParmC = params;

		if (ParmC.m_VS) *((UINT64*)&ParmC.m_VS) = params.m_VS->GetHash();
		if (ParmC.m_HS) *((UINT64*)&ParmC.m_HS) = params.m_HS->GetHash();
		if (ParmC.m_DS) *((UINT64*)&ParmC.m_DS) = params.m_DS->GetHash();
		if (ParmC.m_GS) *((UINT64*)&ParmC.m_GS) = params.m_GS->GetHash();
		if (ParmC.m_PS) *((UINT64*)&ParmC.m_PS) = params.m_PS->GetHash();

		DX12_ASSERT(!memcmp(&ParmC, &Parm, sizeof(Parm)), "Hash collision!");
#endif
		return;
	}

	// Now find a RootSignature with the same mapping
	{
		SResourceMappings resourceMappings;

		if (bGfx)
		{
			resourceMappings.Init(params.m_VS, params.m_HS, params.m_DS, params.m_GS, params.m_PS);
		}
		else
		{
			resourceMappings.Init(params.m_CS);
		}

		// NOTE: Don't base the RootSignature cache-lookup on the shaders in any way
		GetOrCreateRootSignature(resourceMappings, bGfx, nodeMask, result);
		if (!result)
			return;
	}

	m_RootSignatureMapL0[hash] = result;
}

void CRootSignatureCache::GetOrCreateRootSignature(const SResourceMappings& params, bool bGfx, UINT nodeMask, DX12_PTR(CRootSignature)& result)
{
	// NOTE: Hash the contents of the mapping instead of the pointers (which can change)
	SResourceMappings Parm = params;
	Parm.ConvertPointersToIndices();

	// LSB filled marks compute pipeline states, in case graphics and compute hashes collide
	THash hash = (bGfx ? 1 : 0) | ((~1) & ComputeSmallHash<sizeof(SResourceMappings)>(&Parm));
	TRootSignatureMap::iterator iter = m_RootSignatureMapL1.find(hash);

	if (iter != m_RootSignatureMapL1.end())
	{
		result = iter->second;

#ifdef DEBUG
		SResourceMappings ParmC = result.get()->GetResourceMappings();
		ParmC.ConvertPointersToIndices();
		DX12_ASSERT(!memcmp(&ParmC, &Parm, sizeof(Parm)), "Hash collision!");
#endif // !RELEASE
		return;
	}

	result = new CRootSignature(m_pDevice);
	if (!result->Init(params, bGfx, nodeMask))
	{
		DX12_ERROR("Could not create root signature!");
		result = NULL;
		return;
	}

	m_RootSignatureMapL1[hash] = result;
}

}
