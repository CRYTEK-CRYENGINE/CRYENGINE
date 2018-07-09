// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12Shader.hpp"

namespace NCryDX12
{

struct SResourceMappings
{
	// Map of all <stage,slot> pairs to descriptor-tables offsets (the CBV+SRV+UAV/SMP index is the offset)
	struct ShaderInputMapping
	{
		D3D12_DESCRIPTOR_RANGE_TYPE ViewType;
		EShaderStage                ShaderStage;
		uint8_t                     ShaderSlot;
	#ifdef _DEBUG
		uint8_t                     DescriptorOffset;
	#endif
	};

	// Order matters
	ShaderInputMapping m_RESs[128];
	UINT               m_NumRESs;
	ShaderInputMapping m_SMPs[32];
	UINT               m_NumSMPs;

	// Memory holding all descriptor-ranges which will be added to the root-signature (basically a heap)
	CD3DX12_DESCRIPTOR_RANGE  m_DescRanges[128];
	UINT                      m_DescRangeCursor;
	// Consecutive list of all root-parameter descriptions of all types (points to m_DescRanges)
	CD3DX12_ROOT_PARAMETER    m_RootParameters[64];
	UINT                      m_NumRootParameters;
	// Static samplers, directly embedded in the shaders once PSO is created with corresponding root signature
	D3D12_STATIC_SAMPLER_DESC m_StaticSamplers[16];
	UINT                      m_NumStaticSamplers;

	// Consecutive list of all descriptors which are in the root-signature (without holes)
	EShaderStage m_ConstantViews[8 /*CB_NUM*/];
	UINT         m_NumConstantViews;

	// Consecutive list of all descriptor-tables which are in the root-signature (without holes)
	struct SDescriptorTableInfo
	{
		D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
		UINT                       m_Offset;
	};

	SDescriptorTableInfo m_DescriptorTables[64];
	UINT                 m_NumDescriptorTables;

	// Total number of resources bound to all shader stages (CBV+SRV+UAV)
	size_t m_NumResources;

	// Total number of samplers bound to all shader stages
	size_t m_NumDynamicSamplers;

	SResourceMappings()
	{
		// because of alignment there can be 0xCCCCCCCC
		ZeroStruct(*this);
	}

	SResourceMappings(const SResourceMappings& other)
	{
		*this = other;
	}

	void ConvertPointersToIndices() /* for Hashing */
	{
		// patch root parameters pointing to an offset
		for (int i = 0; i < m_NumRootParameters; ++i)
		{
			if (m_RootParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				auto idx = std::distance<const D3D12_DESCRIPTOR_RANGE*>(&m_DescRanges[0], m_RootParameters[i].DescriptorTable.pDescriptorRanges);
				m_RootParameters[i].DescriptorTable.pDescriptorRanges = (D3D12_DESCRIPTOR_RANGE*)idx;
			}
		}
	}

	SResourceMappings& operator=(const SResourceMappings& other)
	{
		memcpy(this, &other, sizeof(*this));

		// patch root parameters pointing to m_DescRanges
		for (int i = 0; i < m_NumRootParameters; ++i)
		{
			if (other.m_RootParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				auto idx = std::distance<const D3D12_DESCRIPTOR_RANGE*>(&other.m_DescRanges[0], other.m_RootParameters[i].DescriptorTable.pDescriptorRanges);
				m_RootParameters[i].DescriptorTable.pDescriptorRanges = &m_DescRanges[idx];
			}
		}

		return *this;
	}

	void Init(CShader* vs, CShader* hs, CShader* ds, CShader* gs, CShader* ps);
	void Init(CShader* cs);

	void Clear();
	void Append(CShader* pShader, EShaderStage eStage);
	void Append(UINT currentRangeCursor, UINT& lastRangeCursor, UINT currentOffset, UINT& lastOffset, D3D12_DESCRIPTOR_HEAP_TYPE eHeap, D3D12_SHADER_VISIBILITY eVisibility);

	void DebugPrint() const;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CRootSignature : public CDeviceObject
{
public:
	struct SInitParams
	{
		CShader* m_VS;
		CShader* m_HS;
		CShader* m_DS;
		CShader* m_GS;
		CShader* m_PS;
		CShader* m_CS;
	};

	CRootSignature(CDevice* device);
	~CRootSignature();

	bool        Init(const SResourceMappings& resourceMappings, bool bGfx, UINT nodeMask);

	ILINE THash GetHash() const
	{
		return m_Hash;
	}

	static CShader* ConvertToHash(const CShader* shader)
	{
		CShader* ret = nullptr;

		if (shader)
		{
			*((uint64*)&ret) = shader->GetHash();
		}

		return ret;
	}

	ILINE const SResourceMappings& GetResourceMappings() const
	{
		return m_ResourceMappings;
	}

	ILINE ID3D12RootSignature* GetD3D12RootSignature() const
	{
		return /*PassAddRef*/ (m_pRootSignature);
	}

private:
	bool Serialize(bool bGfx, UINT nodeMask);

	UINT              m_nodeMask;
	THash             m_Hash;
	DX12_PTR(ID3D12RootSignature) m_pRootSignature;
	SResourceMappings m_ResourceMappings;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CRootSignatureCache
{
public:
	CRootSignatureCache();
	~CRootSignatureCache();

	bool Init(CDevice* device);

	void GetOrCreateRootSignature(const CRootSignature::SInitParams & params, bool bGfx, UINT nodeMask, DX12_PTR(CRootSignature) & result);
	void GetOrCreateRootSignature(const SResourceMappings &params, bool bGfx, UINT nodeMask, DX12_PTR(CRootSignature) & result);

private:
	CDevice* m_pDevice;

	typedef std::unordered_map<THash, DX12_PTR(CRootSignature)> TRootSignatureMap;
	TRootSignatureMap m_RootSignatureMapL0;
	TRootSignatureMap m_RootSignatureMapL1;
};

}
