// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

const uint32 MAX_PIPELINE_SCENE_STAGES = 4;
const uint32 MAX_PIPELINE_SCENE_STAGE_PASSES = 5;

struct SGraphicsPipelineStateDescription
{
	SShaderItem        shaderItem;
	EShaderTechniqueID technique;
	int                _dummy;       // Just for packing so memcmp/hash works
	uint64             objectFlags;
	uint64             objectRuntimeMask;
	uint32             objectFlags_MDV;
	EVertexFormat      vertexFormat;
	uint32             streamMask;
	int                primitiveType;

	SGraphicsPipelineStateDescription()
		: technique(TTYPE_Z)
		, objectFlags(0)
		, objectFlags_MDV(0)
		, objectRuntimeMask(0)
		, vertexFormat(eVF_Unknown)
		, streamMask(0)
		, primitiveType(0)
		, _dummy(0)
	{};
	SGraphicsPipelineStateDescription(CRenderObject* pObj, const SShaderItem& shaderItem, EShaderTechniqueID technique, EVertexFormat vertexFormat, uint32 streamMask, int primitiveType);

	bool operator==(const SGraphicsPipelineStateDescription& other) const
	{
		return 0 == memcmp(this, &other, sizeof(*this));
	}
};

static_assert(sizeof(SGraphicsPipelineStateDescription) == sizeof(SShaderItem) + 6 * sizeof(uint32) + 2 * sizeof(uint64), "There may be no padding in SGraphicsPipelineStateDescription!");

struct SComputePipelineStateDescription
{
	SShaderItem        shaderItem;
	EShaderTechniqueID technique;
	uint64             objectFlags;
	uint64             objectRuntimeMask;
	uint32             objectFlags_MDV;

	SComputePipelineStateDescription()
		: technique(TTYPE_Z)
		, objectFlags(0)
		, objectFlags_MDV(0)
		, objectRuntimeMask(0)
	{};
	SComputePipelineStateDescription(CRenderObject* pObj, const SShaderItem& shaderItem, EShaderTechniqueID technique);

	bool operator==(const SComputePipelineStateDescription& other) const
	{
		return 0 == memcmp(this, &other, sizeof(*this));
	}
};

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
