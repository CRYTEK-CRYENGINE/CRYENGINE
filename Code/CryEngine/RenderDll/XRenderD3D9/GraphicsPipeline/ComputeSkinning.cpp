// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ComputeSkinning.h"
#include "../../Common/ComputeSkinningStorage.h"
#include "../../Common/RenderPipeline.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

namespace compute_skinning
{
const int kMaterialFlagComputeSkinningWithMorphs = 0x04;

// this is current not used, but maybe it will be in the future,
// as an additional flag to the skin attachment
// if it is useful
const int kMaterialFlagComputeSkinningWithMorphsPostSkinning = 0x08;

void SPerMeshResources::PushMorphs(const int numMorphs, const int numMorphsBitField, const Vec4* pMorphDeltas, const uint64* pMorphsBitField)
{
	morphsDeltas.UpdateBufferContent(pMorphDeltas, numMorphs);
	morphsBitField.UpdateBufferContent(pMorphsBitField, numMorphsBitField);

	uploadState |= sState_MorphsInitialized;
}

void SPerMeshResources::PushBindPoseBuffers(const int numVertices, const int numIndices, const int numAdjTriangles, const compute_skinning::SSkinVertexIn* vertices, const vtx_idx* indices, const uint32* adjTris)
{
	verticesIn.UpdateBufferContent(vertices, numVertices);
	indicesIn.UpdateBufferContent(indices, numIndices);
	adjTriangles.UpdateBufferContent(adjTris, numAdjTriangles);

	uploadState |= sState_PosesInitialized;
}

void SPerMeshResources::PushWeights(const int numWeights, const int numWeightsMap, const compute_skinning::SSkinning* w, const compute_skinning::SSkinningMap* weightsMap)
{
	skinningVector.UpdateBufferContent(w, numWeights);
	skinningVectorMap.UpdateBufferContent(weightsMap, numWeightsMap);

	uploadState |= sState_WeightsInitialized;
}

size_t SPerMeshResources::GetSizeBytes()
{
	size_t total = 0;
	total += sizeof(compute_skinning::SSkinning) * skinningVector.GetSize();
	total += sizeof(compute_skinning::SSkinningMap) * skinningVectorMap.GetSize();
	total += sizeof(compute_skinning::SSkinVertexIn) * verticesIn.GetSize();
	total += sizeof(vtx_idx) * verticesIn.GetSize();
	total += sizeof(uint32) * adjTriangles.GetSize();
	total += sizeof(Vec4) * morphsDeltas.GetSize();
	total += sizeof(uint64) * morphsBitField.GetSize();
	return total;
}

SPerInstanceResources::SPerInstanceResources(const int numVertices, const int numTriangles)
	: verticesOut(numVertices)
	, tangentsOut(numTriangles)
	, lastFrameInUse(0)
{
	verticesOut.CreateDeviceBuffer();
	tangentsOut.CreateDeviceBuffer();

	CShader* pShader = gcpRendD3D.m_cEF.mfForName("ComputeSkinning", EF_SYSTEM, 0, 0);
	CShader* pShaderWithMorphs = gcpRendD3D.m_cEF.mfForName("ComputeSkinning", EF_SYSTEM, 0, kMaterialFlagComputeSkinningWithMorphs);

	passDeform.SetTechnique(pShader, CCryNameTSCRC("Deform"), 0);
	passDeformWithMorphs.SetTechnique(pShaderWithMorphs, CCryNameTSCRC("Deform"), 0);
	passTriangleTangents.SetTechnique(pShader, CCryNameTSCRC("TriangleTangents"), 0);
	passVertexTangents.SetTechnique(pShader, CCryNameTSCRC("VertexTangents"), 0);

	passDeform.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	passDeformWithMorphs.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	passTriangleTangents.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	passVertexTangents.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
}

SPerInstanceResources::~SPerInstanceResources()
{
	verticesOut.FreeDeviceBuffer();
	tangentsOut.FreeDeviceBuffer();

	passDeform.Reset();
	passDeformWithMorphs.Reset();
	passTriangleTangents.Reset();
	passVertexTangents.Reset();
}

size_t SPerInstanceResources::GetSizeBytes()
{
	size_t total = 0;
	total += sizeof(SComputeShaderSkinVertexOut) * verticesOut.GetSize();
	total += sizeof(SComputeShaderTriangleNT) * tangentsOut.GetSize();
	return total;
}

std::shared_ptr<compute_skinning::IPerMeshDataSupply> CStorage::GetOrCreateComputeSkinningPerMeshData(const CRenderMesh* pMesh)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_csMesh);
	auto it = m_perMeshResources.find(pMesh);
	if (it == m_perMeshResources.end())
	{
		std::shared_ptr<SPerMeshResources> element(std::make_shared<SPerMeshResources>());
		m_perMeshResources[pMesh] = element;
	}
	return m_perMeshResources[pMesh];
}

std::shared_ptr<SPerInstanceResources> CStorage::GetOrCreatePerInstanceResources(int64 frameId,const void* pCustomTag, const int numVertices, const int numTriangles)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_csInstance);
	auto it = m_perInstanceResources.find(pCustomTag);
	if (it == m_perInstanceResources.end())
	{
		std::shared_ptr<SPerInstanceResources> element(std::make_shared<SPerInstanceResources>(numVertices, numTriangles));
		it = m_perInstanceResources.insert(std::make_pair(pCustomTag, element)).first;
	}

	it->second->lastFrameInUse = frameId;
	return it->second;
}

std::shared_ptr<SPerMeshResources> CStorage::GetPerMeshResources(CRenderMesh* pMesh)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_csMesh);
	auto it = m_perMeshResources.find(pMesh);
	if (it != m_perMeshResources.end())
		return m_perMeshResources[pMesh];

	// return empty ptr
	return std::shared_ptr<SPerMeshResources>();
}

void CStorage::RetirePerMeshResources()
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_csMesh);
	// lock this
	for (auto iter = m_perMeshResources.begin(); iter != m_perMeshResources.end(); )
	{
		const std::shared_ptr<SPerMeshResources>& pRes = iter->second;
		const CRenderMesh* pMesh = iter->first;
		++iter;
		if (pRes.use_count() == 1)
			m_perMeshResources.erase(pMesh);
	}
}

void CStorage::RetirePerInstanceResources(int64 frameId)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_csInstance);
	const int64 curFrameID = frameId;

	for (auto iter = m_perInstanceResources.begin(); iter != m_perInstanceResources.end(); )
	{
		const std::shared_ptr<SPerInstanceResources>& pRes = iter->second;
		const void* pCustom = iter->first;
		++iter;
		if (pRes.use_count() == 1 && pRes->lastFrameInUse < curFrameID - 1)
			m_perInstanceResources.erase(pCustom);
	}
}

void CStorage::DebugDraw()
{
	ColorF c0 = ColorF(0.0f, 1.0f, 1.0f, 1.0f);
	ColorF c1 = ColorF(0.0f, 0.8f, 0.8f, 1.0f);

	uint32 totalPerInstance = 0;

	for (auto iter = m_perInstanceResources.begin(); iter != m_perInstanceResources.end(); ++iter)
	{
		totalPerInstance += iter->second->GetSizeBytes();
	}

	uint32 totalPerMesh = 0;

	for (auto iter = m_perMeshResources.begin(); iter != m_perMeshResources.end(); ++iter)
	{
		totalPerMesh += iter->second->GetSizeBytes();
	}

	IRenderAuxText::Draw2dLabel(1, 20.0f, 2.0f, c1, false, "[Compute Skinning]");
	IRenderAuxText::Draw2dLabel(1, 40.0f, 1.5f, c1, false, " Instances: %i, Total GPU memory for Instance Buffers: (%iKB)", m_perInstanceResources.size(), totalPerInstance / 1024);
	IRenderAuxText::Draw2dLabel(1, 60.0f, 1.5f, c1, false, " Meshes: %i, Total GPU memory for Mesh Buffers: (%iKB)", m_perMeshResources.size(), totalPerMesh / 1024);
}

CGpuBuffer* CStorage::GetOutputVertices(const void* pCustomTag)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_csInstance);
	auto it = m_perInstanceResources.find(pCustomTag);
	if (it != m_perInstanceResources.end())
		return &it->second->verticesOut.GetBuffer();
	else
		return nullptr;
}

}

CComputeSkinningStage::CComputeSkinningStage()
{
}

// Update runs before render objects are compiled, which gives us the opportunity to
// substitute retired/emerging resources for the compiled per-instance resource-sets
void CComputeSkinningStage::Update()
{
	auto frameId = RenderView()->GetFrameId();

#if !defined(_RELEASE) // !NDEBUG
	int32 CurrentFrameID = frameId;
	if (CurrentFrameID == m_oldFrameIdExecute)
		return;
	m_oldFrameIdExecute = CurrentFrameID;
#endif

	// Delete resources which weren't used last frame.
	m_storage.RetirePerMeshResources();
	m_storage.RetirePerInstanceResources(frameId);

	// TODO:/NOTE: possibly multi-threadable recording
	auto* pList = RenderView()->GetSkinningDataPools().pDataComputeSkinning;
	for (auto iter = pList->begin(); iter != pList->end(); ++iter)
	{
		SSkinningData* pSD = *iter;
		CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(pSD->pRenderMesh);

		auto mr = m_storage.GetPerMeshResources(pRenderMesh);

		if (!mr || !mr->IsInitialized(compute_skinning::SPerMeshResources::sState_PosesInitialized | compute_skinning::SPerMeshResources::sState_WeightsInitialized))
		{
			// the mesh didn't get any compute skinning resources allocated at loading time
			// compute skinning can not be performed on this instance
			// or the mesh did not supply all required data yet
			continue;
		}

		auto ir = m_storage.GetOrCreatePerInstanceResources(frameId, pSD->pCustomTag, mr->indicesIn.GetSize(), mr->indicesIn.GetSize() / 3);
	}
}

void CComputeSkinningStage::Prepare()
{
	CDeviceCommandListRef pCoreInterface(GetDeviceObjectFactory().GetCoreCommandList());

	// TODO:/NOTE: possibly multi-threadable recording
	auto frameId = RenderView()->GetFrameId();
	auto* pList = RenderView()->GetSkinningDataPools().pDataComputeSkinning;
	for (auto iter = pList->begin(); iter != pList->end(); ++iter)
	{
		SSkinningData* pSD = *iter;
		CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(pSD->pRenderMesh);

		auto mr = m_storage.GetPerMeshResources(pRenderMesh);

		if (!mr || !mr->IsInitialized(compute_skinning::SPerMeshResources::sState_PosesInitialized | compute_skinning::SPerMeshResources::sState_WeightsInitialized))
		{
			// the mesh didn't get any compute skinning resources allocated at loading time
			// compute skinning can not be performed on this instance
			// or the mesh did not supply all required data yet
			continue;
		}

		auto ir = m_storage.GetOrCreatePerInstanceResources(frameId, pSD->pCustomTag, mr->indicesIn.GetSize(), mr->indicesIn.GetSize() / 3);
		
		bool bDoPreMorphs = (pSD->nHWSkinningFlags & eHWS_DC_Deformation_PreMorphs ? true : false) && pRenderMesh->m_nMorphs;
		bool bDoTangents  = (pSD->nHWSkinningFlags & eHWS_DC_Deformation_Tangents  ? true : false);
		
		uint32 numTriangles = pRenderMesh->GetIndicesCount() / 3;
		int    vertexCount  = pRenderMesh->GetVerticesCount();

		////////////////////////////////////////////////////////////////////////////////////////////
		// bind output skinning
		ir->passDeform          .SetOutputUAV(0, &ir->verticesOut.GetBuffer());
		ir->passDeformWithMorphs.SetOutputUAV(0, &ir->verticesOut.GetBuffer());

		// bind input skinning buffers
		ir->passDeform.SetBuffer(0, &mr->verticesIn.GetBuffer());
		ir->passDeform.SetBuffer(1, &mr->skinningVector.GetBuffer());
		ir->passDeform.SetBuffer(2, &mr->skinningVectorMap.GetBuffer());

		ir->passDeformWithMorphs.SetBuffer(0, &mr->verticesIn.GetBuffer());
		ir->passDeformWithMorphs.SetBuffer(1, &mr->skinningVector.GetBuffer());
		ir->passDeformWithMorphs.SetBuffer(2, &mr->skinningVectorMap.GetBuffer());

		// bind transform bones
		ir->passDeform          .SetInlineConstantBuffer(eConstantBufferShaderSlot_SkinQuat, alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSD->pCharInstCB)->boneTransformsBuffer);
		ir->passDeformWithMorphs.SetInlineConstantBuffer(eConstantBufferShaderSlot_SkinQuat, alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSD->pCharInstCB)->boneTransformsBuffer);

		CD3D9Renderer::SCharacterInstanceCB* cicb = alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSD->pCharInstCB);

		// bind morphs if needed
		if (bDoPreMorphs)
		{
			ir->passDeformWithMorphs.SetBuffer(3, &mr->morphsDeltas.GetBuffer());
			ir->passDeformWithMorphs.SetBuffer(4, &mr->morphsBitField.GetBuffer());
			ir->passDeformWithMorphs.SetBuffer(5, &cicb->activeMorphsBuffer);
		}

		// bind tangents if needed
		if (bDoTangents)
		{
			{
				// input
				ir->passTriangleTangents.SetBuffer(6, &mr->indicesIn.GetBuffer());
				// output
				ir->passTriangleTangents.SetOutputUAV(0, &ir->verticesOut.GetBuffer());
				ir->passTriangleTangents.SetOutputUAV(1, &ir->tangentsOut.GetBuffer());
			}

			{
				ir->passVertexTangents.SetBuffer(0, &mr->verticesIn.GetBuffer());
				ir->passVertexTangents.SetBuffer(7, &mr->adjTriangles.GetBuffer());
				ir->passVertexTangents.SetOutputUAV(0, &ir->verticesOut.GetBuffer());
				ir->passVertexTangents.SetOutputUAV(1, &ir->tangentsOut.GetBuffer());
			}
		}

		////////////////////////////////////////////////////////////////////////////////////////////
		// dispatch skinning and morphs
		uint numMorphMaskBits = (pRenderMesh->m_nMorphs + 31) & ~31;
		uint numMorphMasks    = numMorphMaskBits >> 5;

		// morphs are applied pre-skinning so for post-skinning morphs, one has to send a trigger
		// to the GPU and replace the #define PRE_MORPH inside the shader with an if()
		const Vec4 params(
			(f32)vertexCount,
			bDoPreMorphs ? (f32)pSD->nNumActiveMorphs : 0,
			(f32)pRenderMesh->m_nMorphs,
			(f32)numMorphMasks);

		// bind morphs if needed
		if (bDoPreMorphs)
		{
			ir->passDeformWithMorphs.BeginConstantUpdate();
			ir->passDeformWithMorphs.SetConstant(CCryNameR("g_DeformDispatchParams"), params);
		}
		else
		{
			ir->passDeform.BeginConstantUpdate();
			ir->passDeform.SetConstant(CCryNameR("g_DeformDispatchParams"), params);
		}

		// bind tangents if needed
		if (bDoTangents)
		{
			{
				// calculate triangles TN
				Vec4 params((f32)numTriangles, 0.f, 0.f, 0.f);

				ir->passTriangleTangents.BeginConstantUpdate();
				ir->passTriangleTangents.SetConstant(CCryNameR("g_DeformDispatchParams"), params);
			}

			{
				// calculate vertices TN
				Vec4 params((f32)vertexCount, 0.f, 0.f, 0.f);

				ir->passVertexTangents.BeginConstantUpdate();
				ir->passVertexTangents.SetConstant(CCryNameR("g_DeformDispatchParams"), params);
			}
		}

		////////////////////////////////////////////////////////////////////////////////////////////
		// bind morphs if needed
		if (bDoPreMorphs)
		{
			ir->passDeformWithMorphs.PrepareResourcesForUse(pCoreInterface);
		}
		else
		{
			ir->passDeform.PrepareResourcesForUse(pCoreInterface);
		}

		// bind tangents if needed
		if (bDoTangents)
		{
			{
				ir->passTriangleTangents.PrepareResourcesForUse(pCoreInterface);
			}

			{
				ir->passVertexTangents.PrepareResourcesForUse(pCoreInterface);
			}
		}
	}
}

void CComputeSkinningStage::Execute()
{
	PROFILE_LABEL_SCOPE("CHARACTER_DEFORMATION");

	// Prepare buffers and textures which have been used by pixel shaders for use in the compute queue
	// Reduce resource state switching by requesting the most inclusive resource state
	Prepare();

	{
		const bool bAsynchronousCompute = CRenderer::CV_r_D3D12AsynchronousCompute & BIT((eStage_ComputeSkinning - eStage_FIRST_ASYNC_COMPUTE)) ? true : false;

		// TODO:/NOTE: possibly multi-threadable recording
		auto frameId = RenderView()->GetFrameId();
		auto* pList = RenderView()->GetSkinningDataPools().pDataComputeSkinning;
		for (auto iter = pList->begin(); iter != pList->end(); ++iter)
		{
			// TODO: convert to array of command-lists pattern
			// TODO: profile single command list vs. multiple command lists
			SScopedComputeCommandList pComputeInterface(bAsynchronousCompute);

			SSkinningData* pSD = *iter;
			CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(pSD->pRenderMesh);

			auto mr = m_storage.GetPerMeshResources(pRenderMesh);

			if (!mr || !mr->IsInitialized(compute_skinning::SPerMeshResources::sState_PosesInitialized | compute_skinning::SPerMeshResources::sState_WeightsInitialized))
			{
				// the mesh didn't get any compute skinning resources allocated at loading time
				// compute skinning can not be performed on this instance
				// or the mesh did not supply all required data yet
				continue;
			}

			auto ir = m_storage.GetOrCreatePerInstanceResources(frameId, pSD->pCustomTag, mr->indicesIn.GetSize(), mr->indicesIn.GetSize() / 3);

			bool bDoPreMorphs = (pSD->nHWSkinningFlags & eHWS_DC_Deformation_PreMorphs ? true : false) && pRenderMesh->m_nMorphs;
			bool bDoTangents  = (pSD->nHWSkinningFlags & eHWS_DC_Deformation_Tangents  ? true : false);
			
			uint32 numTriangles = pRenderMesh->GetIndicesCount() / 3;
			int    vertexCount  = pRenderMesh->GetVerticesCount();

			////////////////////////////////////////////////////////////////////////////////////////////
			// dispatch morphs if needed
			if (bDoPreMorphs)
			{
				ir->passDeformWithMorphs.SetDispatchSize((vertexCount + 63) >> 6, 1, 1);
				ir->passDeformWithMorphs.Execute(pComputeInterface);
			}
			else
			{
				ir->passDeform.SetDispatchSize((vertexCount + 63) >> 6, 1, 1);
				ir->passDeform.Execute(pComputeInterface);
			}

			// dispatch tangents
			if (bDoTangents)
			{
				{
					ir->passTriangleTangents.SetDispatchSize((numTriangles + 63) >> 6, 1, 1);
					ir->passTriangleTangents.Execute(pComputeInterface);
				}

				{
					ir->passVertexTangents.SetDispatchSize((vertexCount + 63) >> 6, 1, 1);
					ir->passVertexTangents.Execute(pComputeInterface);
				}
			}
		}
	}

	static ICVar* cvar_gdDebugDraw = gEnv->pConsole->GetCVar("r_ComputeSkinningDebugDraw");
	if (cvar_gdDebugDraw && cvar_gdDebugDraw->GetIVal())
		m_storage.DebugDraw();
}

void CComputeSkinningStage::PreDraw()
{
	CDeviceCommandListRef pCoreInterface(GetDeviceObjectFactory().GetCoreCommandList());

	std::vector<CGpuBuffer*> UAVs;

	// TODO:/NOTE: possibly multi-threadable recording
	auto frameId = RenderView()->GetFrameId();
	auto* pList = RenderView()->GetSkinningDataPools().pDataComputeSkinning;
	for (auto iter = pList->begin(); iter != pList->end(); ++iter)
	{
		SSkinningData* pSD = *iter;
		CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(pSD->pRenderMesh);

		auto mr = m_storage.GetPerMeshResources(pRenderMesh);

		if (!mr || !mr->IsInitialized(compute_skinning::SPerMeshResources::sState_PosesInitialized | compute_skinning::SPerMeshResources::sState_WeightsInitialized))
		{
			// the mesh didn't get any compute skinning resources allocated at loading time
			// compute skinning can not be performed on this instance
			// or the mesh did not supply all required data yet
			continue;
		}

		auto ir = m_storage.GetOrCreatePerInstanceResources(frameId, pSD->pCustomTag, mr->indicesIn.GetSize(), mr->indicesIn.GetSize() / 3);

		bool bDoTangents = (pSD->nHWSkinningFlags & eHWS_DC_Deformation_Tangents ? true : false);

		////////////////////////////////////////////////////////////////////////////////////////////
		UAVs.emplace_back(&ir->verticesOut.GetBuffer());

		if (bDoTangents)
		{
			UAVs.emplace_back(&ir->tangentsOut.GetBuffer());
		}
	}

	// Prepare skinning buffers which have been used in the compute shader for vertex use
	if(!UAVs.empty())
		pCoreInterface.GetGraphicsInterface()->PrepareUAVsForUse(UAVs.size(), &UAVs[0], false);
}
