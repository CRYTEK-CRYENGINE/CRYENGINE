// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../../DeviceManager/DeviceResourceSet.h"

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
class CDeviceResourceLayout_DX11 : public CDeviceResourceLayout
{
public:
	CDeviceResourceLayout_DX11(UsedBindSlotSet requiredResourceBindings)
		: CDeviceResourceLayout(requiredResourceBindings)
	{}
};
