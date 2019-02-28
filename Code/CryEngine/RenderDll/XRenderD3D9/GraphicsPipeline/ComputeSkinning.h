// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Gpu/GpuComputeBackend.h"
#include "Common/ComputeSkinningStorage.h"
#include "Common/GraphicsPipelineStage.h"
#include "GraphicsPipeline/Common/ComputeRenderPass.h"

namespace compute_skinning
{

#pragma pack(push)
#pragma pack(1)

struct SComputeShaderSkinVertexOut
{
	Vec3  pos;
	float pad1;
	Quat  qtangent;
	Vec3  tangent;
	float pad2;
	Vec3  bitangent;
	float pad3;
	Vec2  uv;
	float pad4[2];
};

// check ORBIS
struct SIndices
{
	uint32 ind;
};

struct SComputeShaderTriangleNT
{
	Vec3 normal;
	Vec3 tangent;
};

struct SMorphsDeltas
{
	Vec4 delta;
};

struct SMorphsBitField
{
	// packed as 32:32
	uint64 maskZoom;
};

#pragma pack(pop)

template<typename T> class CTypedReadResource
{
public:
	CTypedReadResource()
		: m_size(0)
	{}

	CGpuBuffer& GetBuffer() { return m_buffer; };

	void        UpdateBufferContent(const T* pData, size_t nSize) // called from multiple threads: always recreate immutable buffer
	{
		m_buffer.Create(nSize, sizeof(T), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, (const void*)pData);
		m_size = nSize;
	}

	int GetSize() { return m_size; }
private:
	int        m_size;
	CGpuBuffer m_buffer;
};

struct SPerMeshResources : public compute_skinning::IPerMeshDataSupply
{
	CTypedReadResource<compute_skinning::SSkinning>     skinningVector;
	CTypedReadResource<compute_skinning::SSkinningMap>  skinningVectorMap;

	CTypedReadResource<compute_skinning::SSkinVertexIn> verticesIn;
	CTypedReadResource<vtx_idx>                         indicesIn;
	CTypedReadResource<uint32>                          adjTriangles;

	// morph specific
	CTypedReadResource<Vec4>   morphsDeltas;
	CTypedReadResource<uint64> morphsBitField;

	size_t GetSizeBytes();

	enum SState
	{
		sState_NonInitialized     = 0,

		sState_PosesInitialized   = BIT(0),
		sState_WeightsInitialized = BIT(1),
		sState_MorphsInitialized  = BIT(2)
	};

	std::atomic<uint32> uploadState;
	SPerMeshResources() : uploadState(sState_NonInitialized) {}
	bool IsInitialized(uint32 wantedState) const { return (uploadState & wantedState) == wantedState; }

	// per mesh data supply implementation
	virtual void PushMorphs(const int numMorps, const int numMorphsBitField, const Vec4* morphsDeltas, const uint64* morphsBitField) override;
	virtual void PushBindPoseBuffers(const int numVertices, const int numIndices, const int numAdjTriangles, const compute_skinning::SSkinVertexIn* vertices, const vtx_idx* indices, const uint32* adjTriangles) override;
	virtual void PushWeights(const int numWeights, const int numWeightsMap, const compute_skinning::SSkinning* weights, const compute_skinning::SSkinningMap* weightsMap) override;
};

struct SPerInstanceResources
{
	SPerInstanceResources(const int numVertices, const int numTriangles);
	~SPerInstanceResources();

	CComputeRenderPass passDeform;
	CComputeRenderPass passDeformWithMorphs;
	CComputeRenderPass passTriangleTangents;
	CComputeRenderPass passVertexTangents;
	int64              lastFrameInUse;

	size_t             GetSizeBytes();

	// output data
	gpu::CStructuredResource<SComputeShaderSkinVertexOut, gpu::BufferFlagsReadWrite> verticesOut;
	gpu::CStructuredResource<SComputeShaderTriangleNT, gpu::BufferFlagsReadWrite>    tangentsOut;
};

class CStorage : public compute_skinning::IComputeSkinningStorage
{
public:
	std::shared_ptr<IPerMeshDataSupply>    GetOrCreateComputeSkinningPerMeshData(const CRenderMesh* pMesh) override;
	virtual CGpuBuffer*                    GetOutputVertices(const void* pCustomTag) override;

	void                                   RetirePerMeshResources();
	void                                   RetirePerInstanceResources(int64 frameId);
	std::shared_ptr<SPerMeshResources>     GetPerMeshResources(CRenderMesh* pMesh);
	std::shared_ptr<SPerInstanceResources> GetOrCreatePerInstanceResources(int64 frameId,const void* pCustomTag, const int numVertices, const int numTriangles);
	void                                   DebugDraw();

private:
	// needs a lock since this is updated through the streaming thread
	std::unordered_map<const CRenderMesh*, std::shared_ptr<SPerMeshResources>> m_perMeshResources;
	// this is addressed with the pCustomTag, identifying the Skin Attachment
	// this is only accessed through the render thread, so a lock shouldn't be necessary
	std::unordered_map<const void*, std::shared_ptr<SPerInstanceResources>> m_perInstanceResources;

	CryCriticalSectionNonRecursive m_csMesh;
	CryCriticalSectionNonRecursive m_csInstance;
};

}

class CComputeSkinningStage : public CGraphicsPipelineStage
{
public:
	CComputeSkinningStage();

	void Update() final;
	void Prepare();
	void Execute();
	void PreDraw();

	compute_skinning::IComputeSkinningStorage& GetStorage() { return m_storage; }

private:
	compute_skinning::CStorage m_storage;

#if !defined(_RELEASE) // !NDEBUG
	int32                      m_oldFrameIdExecute = -1;
#endif
};
