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
		EConstantBufferShaderSlot slot;
	};

	struct SCompiledUAV
	{
		ID3D11UnorderedAccessView* pUav;
		EHWShaderClass shaderStage;
		int slot;
	};

	CDeviceResourceSet_DX11(CDeviceResourceSet::EFlags flags)
		: CDeviceResourceSet(flags)
	{}

	virtual bool UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags);

	void         ClearCompiledData();

	// set via reflection from shader
	std::array<std::array<ID3D11ShaderResourceView*, MAX_TMU>, eHWSC_Num>                  compiledSRVs;
	std::array<uint8, eHWSC_Num> firstCompiledSRVs, lastCompiledSRVs;

	std::array<std::array<ID3D11SamplerState*, MAX_TMU>, eHWSC_Num>                        compiledSamplers;
	std::array<uint8, eHWSC_Num> firstCompiledSamplers, lastCompiledSamplers;

	// set directly
	std::array<std::array<SCompiledConstantBuffer, eConstantBufferShaderSlot_Count>, eHWSC_Num> compiledCBs;
	std::array<uint8, eHWSC_Num> numCompiledCBs;

	std::array<SCompiledUAV, D3D11_PS_CS_UAV_REGISTER_COUNT> compiledUAVs;
	uint8 numCompiledUAVs;
};

class CDeviceResourceLayout_DX11 : public CDeviceResourceLayout
{
public:
	CDeviceResourceLayout_DX11(UsedBindSlotSet requiredResourceBindings)
		: CDeviceResourceLayout(requiredResourceBindings)
	{}
};
