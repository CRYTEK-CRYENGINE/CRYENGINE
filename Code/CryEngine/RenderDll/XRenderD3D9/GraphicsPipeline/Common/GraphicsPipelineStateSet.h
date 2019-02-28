// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "../../DeviceManager/DeviceObjects.h" // CDeviceGraphicsPSOPtr, CDeviceGraphicsPSOWPtr

const uint32 MAX_PIPELINE_SCENE_STAGE_PASSES = 5;

struct SGraphicsPipelineStateDescription
{
	SShaderItem          shaderItem;
	EShaderTechniqueID   technique;
	uint32               objectFlags_MDV;
	uint64               objectFlags;
	uint64               objectRuntimeMask;
	uint32               streamMask;
	ERenderPrimitiveType primitiveType;
	uint8                renderState;
	InputLayoutHandle    vertexFormat;

	SGraphicsPipelineStateDescription()
		: technique(TTYPE_Z)
		, renderState(0)
		, objectFlags(0)
		, objectFlags_MDV(0)
		, objectRuntimeMask(0)
		, vertexFormat(InputLayoutHandle::Unspecified)
		, streamMask(0)
		, primitiveType(eptUnknown)
	{};
	SGraphicsPipelineStateDescription(CRenderObject* pObj, CRenderElement* pRE, const SShaderItem& shaderItem, EShaderTechniqueID technique, InputLayoutHandle vertexFormat, uint32 streamMask, ERenderPrimitiveType primitiveType);

	bool operator==(const SGraphicsPipelineStateDescription& other) const
	{
		return 0 == memcmp(this, &other, sizeof(*this));
	}
};

static_assert(
	sizeof(SGraphicsPipelineStateDescription) ==
	sizeof(SGraphicsPipelineStateDescription::shaderItem) +
	sizeof(SGraphicsPipelineStateDescription::technique) + 
	sizeof(SGraphicsPipelineStateDescription::objectFlags_MDV) +
	sizeof(SGraphicsPipelineStateDescription::objectFlags) +
	sizeof(SGraphicsPipelineStateDescription::objectRuntimeMask) +
	sizeof(SGraphicsPipelineStateDescription::streamMask) +
	sizeof(SGraphicsPipelineStateDescription::primitiveType) +
	sizeof(SGraphicsPipelineStateDescription::renderState) +
	sizeof(SGraphicsPipelineStateDescription::vertexFormat),
	"There may be no padding in SGraphicsPipelineStateDescription! memcpy/hash might not work.");

// Array of pass id and PipelineState
typedef std::array<CDeviceGraphicsPSOPtr, MAX_PIPELINE_SCENE_STAGE_PASSES> DevicePipelineStatesArray;

// Set of precomputed Pipeline States
class CGraphicsPipelineStateLocalCache
{
public:
	bool Find(const SGraphicsPipelineStateDescription& desc, DevicePipelineStatesArray& out);
	void Put(const SGraphicsPipelineStateDescription& desc, const DevicePipelineStatesArray& states);
	void Clear() { m_states.clear(); }

private:
	CDeviceGraphicsPSOWPtr FindState(uint64 stateHashKey) const;
	uint64                 GetHashKey(const SGraphicsPipelineStateDescription& desc) const;

private:
	struct CachedState
	{
		uint64                            stateHashKey;
		SGraphicsPipelineStateDescription description;
		DevicePipelineStatesArray         m_pipelineStates;
	};
	std::vector<CachedState> m_states;
};

typedef std::shared_ptr<CGraphicsPipelineStateLocalCache> CGraphicsPipelineStateLocalCachePtr;
