// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "../../DeviceManager/DeviceObjects.h" // CDeviceGraphicsPSOPtr, CDeviceGraphicsPSOWPtr

const uint32 MAX_PIPELINE_SCENE_STAGE_PASSES = 6;

struct SGraphicsPipelineStateDescription
{
	SShaderItem          shaderItem;
	uint64               objectRuntimeMask;
	uint64               objectFlags;
	EShaderTechniqueID   technique;
	EVertexModifier      objectFlags_MDV;
	EStreamMasks         streamMask;
	ERenderPrimitiveType primitiveType;
	InputLayoutHandle    vertexFormat;
	uint8                renderState;

	uint32               padding;

	SGraphicsPipelineStateDescription()
		: objectRuntimeMask(0)
		, objectFlags(0)
		, technique(TTYPE_Z)
		, objectFlags_MDV(MDV_NONE)
		, streamMask(VSM_NONE)
		, primitiveType(eptUnknown)
		, vertexFormat(InputLayoutHandle::Unspecified)
		, renderState(0)
		, padding(0)
	{}
	SGraphicsPipelineStateDescription(CRenderObject* pObj, uint64 objFlags, ERenderElementFlags elmFlags, const SShaderItem& shaderItem, EShaderTechniqueID technique, InputLayoutHandle vertexFormat, EStreamMasks streamMask, ERenderPrimitiveType primitiveType);

	bool operator==(const SGraphicsPipelineStateDescription& other) const
	{
		return 0 == memcmp(this, &other, sizeof(*this));
	}
};

static_assert(
	sizeof(SGraphicsPipelineStateDescription) ==
	sizeof(SGraphicsPipelineStateDescription::shaderItem) +
	sizeof(SGraphicsPipelineStateDescription::technique) + 
	sizeof(SGraphicsPipelineStateDescription::objectRuntimeMask) +
	sizeof(SGraphicsPipelineStateDescription::objectFlags) +
	sizeof(SGraphicsPipelineStateDescription::objectFlags_MDV) +
	sizeof(SGraphicsPipelineStateDescription::streamMask) +
	sizeof(SGraphicsPipelineStateDescription::primitiveType) +
	sizeof(SGraphicsPipelineStateDescription::renderState) +
	sizeof(SGraphicsPipelineStateDescription::vertexFormat) +
	sizeof(SGraphicsPipelineStateDescription::padding),
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
