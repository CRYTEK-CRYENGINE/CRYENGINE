// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12RootSignature.hpp"

namespace NCryDX12
{

class CPSO : public CDeviceObject
{
public:
	CPSO(CDevice* device) : CDeviceObject(device) {}
	~CPSO() {}

	bool  Init(ID3D12PipelineState* pD3D12PipelineState);

	THash GetHash() const
	{
		return m_Hash;
	}

	static D3D12_SHADER_BYTECODE ConvertToHash(const D3D12_SHADER_BYTECODE& byteCode)
	{
		D3D12_SHADER_BYTECODE ret;
		ZeroStruct(ret);

		if (byteCode.pShaderBytecode && byteCode.BytecodeLength)
		{
			*((uint64*)&ret) = XXH64(byteCode.pShaderBytecode, byteCode.BytecodeLength, ~byteCode.BytecodeLength);
		}

		return ret;
	}

	ID3D12PipelineState* GetD3D12PipelineState() const
	{
		return /*PassAddRef*/ (m_pD3D12PipelineState);
	}

private:
	THash m_Hash;
	DX12_PTR(ID3D12PipelineState) m_pD3D12PipelineState;
};

//---------------------------------------------------------------------------------------------------------------------
class CGraphicsPSO : public CPSO
{
public:
	struct SInitParams
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_Desc;
		BOOL                               m_DepthBoundsTestEnable;

		CShader*                           m_VS;
		CShader*                           m_HS;
		CShader*                           m_DS;
		CShader*                           m_GS;
		CShader*                           m_PS;
	};

	CGraphicsPSO(CDevice* device) : CPSO(device) {}
	~CGraphicsPSO() {}

	bool                                Init(const SInitParams& params);
	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetDesc()
	{
		return m_Desc;
	}
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetDesc() const
	{
		return m_Desc;
	}

private:
	D3D12_GRAPHICS_PIPELINE_STATE_DESC m_Desc;
};

//---------------------------------------------------------------------------------------------------------------------
class CComputePSO : public CPSO
{
public:
	struct SInitParams
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC m_Desc;

		CShader*                          m_CS;
	};

	CComputePSO(CDevice* device) : CPSO(device) {}
	~CComputePSO() {}

	bool                               Init(const SInitParams& params);
	
	D3D12_COMPUTE_PIPELINE_STATE_DESC& GetDesc()
	{
		return m_Desc;
	}
	const D3D12_COMPUTE_PIPELINE_STATE_DESC& GetDesc() const
	{
		return m_Desc;
	}

private:
	D3D12_COMPUTE_PIPELINE_STATE_DESC m_Desc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CPSOCache
{
public:
	CPSOCache();
	~CPSOCache();

	bool Init(CDevice* device);

	void GetOrCreatePSO(const CGraphicsPSO::SInitParams & params, DX12_PTR(CGraphicsPSO) & result);
	void GetOrCreatePSO(const CComputePSO::SInitParams & params, DX12_PTR(CComputePSO) & result);

private:
	CDevice* m_pDevice;

	typedef std::unordered_map<THash, DX12_PTR(CPSO)> TPSOMap;
	TPSOMap m_PSOMap;
};

}
