// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12Base.hpp"

namespace NCryDX12
{

////////////////////////////////////////////////////////////////////////////

enum EShaderStage : int8
{
	// Graphics pipeline
	ESS_Vertex,
	ESS_Hull,
	ESS_Domain,
	ESS_Geometry,
	ESS_Pixel,

	// Compute pipeline
	ESS_Compute,

	// Complete pipeline
	ESS_All                = ESS_Compute,

	ESS_LastWithoutCompute = ESS_Compute,
	ESS_Num                = ESS_Compute + 1,
	ESS_None               = -1,
	ESS_First              = ESS_Vertex
};

////////////////////////////////////////////////////////////////////////////

struct SBindRanges
{
	struct SRange
	{
		UINT8  m_Start;
		UINT8  m_Length;
		UINT16 m_bUsed       : 1;
		UINT16 m_bShared     : 1;
		UINT16 m_bUnmergable : 1;

		// NOTE: Only needed for debugging
	#ifdef _DEBUG
		UINT8 m_Types[56];
		UINT8 m_Dimensions[56];
	#endif

		SRange()
			: m_Start(0)
			, m_Length(0)
			, m_bUsed(true)
			, m_bShared(false)
			, m_bUnmergable(false)
		{

		}

		SRange(UINT8 start, UINT8 length, UINT8 type, UINT8 dimension, bool bUsed = true, bool bShared = false, bool bUnmergable = false)
			: m_Start(start)
			, m_Length(length)
			, m_bUsed(bUsed)
			, m_bShared(bShared)
			, m_bUnmergable(bUnmergable)
		{
	#ifdef _DEBUG
			for (size_t i = 0; i < length; ++i)
			{
				m_Types[i] = type;
				m_Dimensions[i] = dimension;
			}
	#endif
		}
	};

	typedef std::vector<SRange> TRanges;
	TRanges m_Ranges;
	size_t  m_TotalLength;

	SBindRanges()
		: m_TotalLength(0)
	{

	}

	void Add(UINT8 start, UINT8 length, UINT8 type, UINT8 dimension, bool bUsed = true, bool bShared = false, bool bUnmergable = false)
	{
		size_t i = 0, S = m_Ranges.size();
		if (!bUnmergable && !bShared)
		{
			for (; i < S; ++i)
			{
				SRange& r = m_Ranges[i];
				if (r.m_bUnmergable || r.m_bShared || (r.m_bUsed != bUsed))
				{
					continue;    // Non-Mergeable
				}

				// Can we join new range with an existing one?
				if (r.m_Start + r.m_Length == start)
				{
	#ifdef _DEBUG
					for (size_t j = 0; j < length; ++j)
					{
						r.m_Types[r.m_Length + j] = type;
						r.m_Dimensions[r.m_Length + j] = dimension;
					}
	#endif

					r.m_Length += length;
					break;
				}
			}
		}

		// No existing range found? Create a new one...
		if (i == S || bUnmergable || bShared)
		{
			m_Ranges.push_back(SRange(start, length, type, dimension, bUsed, bShared, bUnmergable));
		}

		m_TotalLength += length;
	}
};

////////////////////////////////////////////////////////////////////////////

struct SResourceRanges
{
	SBindRanges m_ConstantBuffers; // CBV
	SBindRanges m_InputResources;  // SRV
	SBindRanges m_OutputResources; // UAV
	SBindRanges m_Samplers;        // SMP
};

////////////////////////////////////////////////////////////////////////////

class CShader : public CDeviceObject
{
public:
	// Create new shader using DX11 reflection interface
	static CShader* CreateFromD3D11(CDevice* device, const D3D12_SHADER_BYTECODE& byteCode);

	CShader(CDevice* device);

	const D3D12_SHADER_BYTECODE& GetBytecode() const
	{
		return m_Bytecode;
	}

	UINT64 GetHash() const
	{
		return m_Hash;
	}

	UINT64 ConvertToHash() const
	{
		D3D12_SHADER_BYTECODE byteCode = GetBytecode();

		if (byteCode.pShaderBytecode && byteCode.BytecodeLength)
		{
			return XXH64(byteCode.pShaderBytecode, byteCode.BytecodeLength, ~byteCode.BytecodeLength);
		}

		return 0ULL;
	}

	const SResourceRanges& GetResourceRanges() const
	{
		return m_Ranges;
	}

protected:
	virtual ~CShader();

private:
	D3D12_SHADER_BYTECODE m_Bytecode;
	SResourceRanges       m_Ranges;
	UINT64                m_Hash;
};

}
