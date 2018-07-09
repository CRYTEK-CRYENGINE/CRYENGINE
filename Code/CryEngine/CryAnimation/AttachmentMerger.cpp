// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AttachmentMerger.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "ModelMesh.h"
#include "AttachmentMerged.h"
#include "AttachmentManager.h"
#include "CharacterManager.h"
#include "Model.h"
#include "CharacterInstance.h"

CAttachmentMerger::ShadowChunkRange::ShadowChunkIterator::ShadowChunkIterator(TRenderChunkArray::const_iterator curPos, TRenderChunkArray::const_iterator rangeEnd, IMaterial* pMtl)
	: itCurPos(curPos)
	, itEnd(rangeEnd)
	, isShadowChunk(pMtl)
{
	while (itCurPos != itEnd && !isShadowChunk(*itCurPos))
		++itCurPos;
}

CAttachmentMerger::ShadowChunkRange::ShadowChunkIterator& CAttachmentMerger::ShadowChunkRange::ShadowChunkIterator::operator++()
{
	++itCurPos;

	while (itCurPos != itEnd && !isShadowChunk(*itCurPos))
		++itCurPos;

	return *this;
}

CAttachmentMerger::ShadowChunkRange::ShadowChunkIterator CAttachmentMerger::ShadowChunkRange::begin()
{
	return ShadowChunkIterator(pRenderMesh->GetChunks().begin(), pRenderMesh->GetChunks().end(), pMaterial);
}

CAttachmentMerger::ShadowChunkRange::ShadowChunkIterator CAttachmentMerger::ShadowChunkRange::end()
{
	return ShadowChunkIterator(pRenderMesh->GetChunks().end(), pRenderMesh->GetChunks().end(), pMaterial);
}

CAttachmentMerger::MeshStreams::MeshStreams(IRenderMesh* pRenderMesh, uint32 lockFlags, bool bGetSkinningInfos)
	: pMesh(pRenderMesh)
	, pSkinningInfos(NULL)
{
	pRenderMesh->LockForThreadAccess();

	pIndices = pRenderMesh->GetIndexPtr(lockFlags);
	pPositions = pRenderMesh->GetPosPtrNoCache(nPositionStride, lockFlags);

	if (bGetSkinningInfos)
		pSkinningInfos = pRenderMesh->GetHWSkinPtr(nSkinningInfoStride, lockFlags);
}

CAttachmentMerger::MeshStreams::~MeshStreams()
{
	pMesh->UnlockIndexStream();
	pMesh->UnlockStream(VSF_GENERAL);
	pMesh->UnlockStream(VSF_HWSKIN_INFO);
	pMesh->UnLockForThreadAccess();
}

SVF_W4B_I4S& CAttachmentMerger::MeshStreams::GetSkinningInfo(int nVtxIndex)
{
	CRY_ASSERT(nVtxIndex < pMesh->GetVerticesCount());
	return static_cast<SVF_W4B_I4S*>(pSkinningInfos)[nVtxIndex];
}

vtx_idx& CAttachmentMerger::MeshStreams::GetVertexIndex(int i)
{
	CRY_ASSERT(i < pMesh->GetIndicesCount());
	return pIndices[i];
}

InputLayoutHandle CAttachmentMerger::MeshStreams::GetVertexFormat()
{
	return pMesh->GetVertexFormat();
}

CAttachmentMerger::MergeContext::MergeContext(IAttachment* pAttachment)
{
	memset(this, 0x0, sizeof(MergeContext));
	Update(AttachmentRenderData(pAttachment));
}

CAttachmentMerger::MergeContext::MergeContext(const AttachmentRenderData& renderData)
{
	memset(this, 0x0, sizeof(MergeContext));
	Update(renderData);
}

void CAttachmentMerger::MergeContext::Update(const AttachmentRenderData& renderData)
{
	nAccumulatedBoneCount += renderData.nBoneCount;
	nMaxLodCount = max(nMaxLodCount, renderData.nLodCount);

	uint twoSidedCount = 0;
	uint chunkCount = 0;

	for (int lod = 0; lod < MAX_STATOBJ_LODS_NUM; ++lod)
	{
		auto const& lodData = renderData.GetLod(lod);
		if (lodData.pMesh && lodData.pMaterial)
		{
			nAccumulatedVertexCount[lod] += lodData.pMesh->GetVerticesCount();

			for (auto const& chunk : lodData.GetShadowChunks())
			{
				IMaterial* pSubMtl = lodData.pMaterial->GetSafeSubMtl(chunk.m_nMatID);
				IRenderShaderResources* pShaderResources = pSubMtl->GetShaderItem().m_pShaderResources;

				nAccumulatedIndexCount[lod] += chunk.nNumIndices;

				if (pShaderResources && pShaderResources->IsAlphaTested())
					bAlphaTested = true;

				if ((pSubMtl->GetFlags() & MTL_FLAG_2SIDED) != 0)
					++twoSidedCount;

				++chunkCount;
			}

			if (lodData.pMesh->GetSkinningWeightCount() > 4)
				bEightWeightSkinning = true;
		}
	}

	if (twoSidedCount == 0)
		nTwoSided = eStateOff;
	else if (twoSidedCount == chunkCount)
		nTwoSided = eStateOn;
	else
		nTwoSided = eStateMixed;
}

uint CAttachmentMerger::MergeContext::EstimateRequiredMemory() const
{
	const uint nVertexSize = sizeof(SVF_P3F) + sizeof(SVF_W4B_I4S);
	const uint nIndexSize = sizeof(vtx_idx) * 2;  // index buffer is cached CPU side

	uint nResult = 0;
	for (int lod = 0; lod < nMaxLodCount; ++lod)
	{
		nResult += nAccumulatedVertexCount[lod] * nVertexSize;
		nResult += nAccumulatedIndexCount[lod] * nIndexSize;
	}

	return nResult;
}

CAttachmentMerger::AttachmentRenderData::AttachmentRenderData(IAttachment* pAtt)
{
	memset(this, 0x0, sizeof(AttachmentRenderData));

	if (const IAttachmentObject* pAttachmentObject = pAtt->GetIAttachmentObject())
	{
		if (CAttachmentSKIN* pAttachmentSkin = static_cast<CAttachmentSKIN*>(pAttachmentObject->GetIAttachmentSkin()))
		{
			for (uint lod = 0; lod < MAX_STATOBJ_LODS_NUM; ++lod)
			{
				if (IRenderMesh* pRenderMesh = pAttachmentSkin->m_pModelSkin->GetIRenderMesh(lod))
				{
					arrLods[lod].pMesh = pRenderMesh;
					arrLods[lod].pMaterial = GetAttachmentMaterial(pAtt, lod);
					arrLods[lod].mTransform = Matrix34::CreateTranslationMat(pAttachmentSkin->m_pModelSkin->GetModelMesh(lod)->m_vRenderMeshOffset);
					++nLodCount;
				}
			}

			CSkin* pSkin = static_cast<CSkin*>(pAttachmentSkin->GetISkin());
			nBoneCount = pSkin->m_arrModelJoints.size();
		}
		else if (IStatObj* pStatObj = pAttachmentObject->GetIStatObj())
		{
			for (uint lod = 0; lod < MAX_STATOBJ_LODS_NUM; ++lod)
			{
				if (IStatObj* pStatObjLod = pStatObj->GetLodObject(lod))
				{
					if (IRenderMesh* pRenderMesh = pStatObjLod->GetRenderMesh())
					{
						arrLods[lod].pMesh = pRenderMesh;
						arrLods[lod].pMaterial = GetAttachmentMaterial(pAtt, lod);
						arrLods[lod].mTransform = Matrix34(pAtt->GetAttAbsoluteDefault());
						++nLodCount;
					}
				}
			}

			nBoneCount = 1;
		}
	}

	// propagate last lod data to remaining lods
	if (nLodCount > 0)
	{
		for (int i = nLodCount; i < MAX_STATOBJ_LODS_NUM; ++i)
			arrLods[i] = arrLods[i - 1];
	}

	pAttachment = pAtt;
}

bool CAttachmentMerger::CanMerge(const MergeContext& context1, const MergeContext& context2)
{
	if (context1.nMaxLodCount == 0 || context2.nMaxLodCount == 0) // can always merge with empty attachment
		return true;

	uint nMemorySize = 0;
	for (int lod = 0; lod < MAX_STATOBJ_LODS_NUM; ++lod)
	{
		if (context1.nAccumulatedVertexCount[lod] + context2.nAccumulatedVertexCount[lod] > std::numeric_limits<vtx_idx>::max())
			return false;
	}

	if (context1.nAccumulatedBoneCount + context2.nAccumulatedBoneCount > std::numeric_limits<JointIdType>::max())
		return false;

	if (context1.bAlphaTested || context2.bAlphaTested)
		return false;

	if (context1.nTwoSided == MergeContext::eStateMixed || context2.nTwoSided == MergeContext::eStateMixed)
		return false;

	if (context1.nTwoSided != context2.nTwoSided)
		return false;

	if (context1.bEightWeightSkinning || context2.bEightWeightSkinning)
		return false;

	return true;
}

bool CAttachmentMerger::CanMerge(IAttachment* pAttachment1, IAttachment* pAttachment2)
{
	MergeContext context1(pAttachment1);
	MergeContext context2(pAttachment2);

	return Instance().CanMerge(context1, context2);
}

IMaterial* CAttachmentMerger::GetAttachmentMaterial(const IAttachment* pAttachment, int lod)
{
	const IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
	IMaterial* pMaterial = pAttachmentObject->GetBaseMaterial(lod);
	if (!pMaterial) pMaterial = pAttachmentObject->GetBaseMaterial(0);

	return pMaterial;
}

void CAttachmentMerger::MergeAttachments(DynArray<_smart_ptr<SAttachmentBase>>& arrAttachments, DynArray<_smart_ptr<CAttachmentMerged>>& arrMergedAttachments, CAttachmentManager* pAttachmentManager)
{
	DEFINE_PROFILER_FUNCTION();

	DynArray<AttachmentRenderData> mergeableAttachments;
	mergeableAttachments.reserve(arrAttachments.size());

	// get list of mergeable attachments
	for (auto pAttachment : arrAttachments)
	{
		if (pAttachment->IsMerged() || pAttachment->GetIAttachmentObject() == NULL)
			continue;

		AttachmentRenderData attachmentData(pAttachment);

		bool bAllValidMaterials = true;
		for (auto const& lod : attachmentData.arrLods)
		{
			if (!lod.pMaterial)
			{
				bAllValidMaterials = false;
				break;
			}
		}

		if (!bAllValidMaterials || attachmentData.nLodCount == 0)
			continue;

		mergeableAttachments.push_back(attachmentData);
	}

	// now try to merge
	for (uint i = 0; i < mergeableAttachments.size(); ++i)
	{
		if (mergeableAttachments[i].pAttachment == NULL)
			continue;

		DynArray<AttachmentRenderData> curAttachmentList;
		curAttachmentList.reserve(mergeableAttachments.size());
		curAttachmentList.push_back(mergeableAttachments[i]);

		MergeContext context(mergeableAttachments[i]);
		mergeableAttachments[i].pAttachment = NULL; // mark as done

		for (uint j = i + 1; j < mergeableAttachments.size(); ++j)
		{
			if (mergeableAttachments[j].pAttachment)
			{
				CRY_ASSERT(!mergeableAttachments[j].pAttachment->IsMerged());

				if (CanMerge(mergeableAttachments[j], context))
				{
					MergeContext mergedContext = context;
					mergedContext.Update(mergeableAttachments[j]);

					if (m_nBytesAllocated + mergedContext.EstimateRequiredMemory() < Console::GetInst().ca_AttachmentMergingMemoryBudget)
					{
						curAttachmentList.push_back(mergeableAttachments[j]);
						context = mergedContext;

						mergeableAttachments[j].pAttachment = NULL;
					}
					else
					{
						m_bOutOfMemory = true;
					}
				}
			}
		}

		if (curAttachmentList.size() > 1)
		{
			CAttachmentMerged* pMergeTarget = NULL;

			// try to merge into existing merged attachment first
			for (auto pMergedAttachment : arrMergedAttachments)
			{
				AttachmentRenderData mergedAttachmentData(pMergedAttachment->m_pMergedSkinAttachment);
				if (CanMerge(mergedAttachmentData, context))
				{
					context.nMaxLodCount = max(mergedAttachmentData.nLodCount, context.nMaxLodCount); // adjust lod count
					pMergeTarget = pMergedAttachment;
					pMergeTarget->m_pMergedSkinAttachment->m_pModelSkin->m_arrModelMeshes.resize(context.nMaxLodCount);

					break;
				}
			}

			// need to create a new merged attachment
			if (!pMergeTarget)
			{
				string attachmentName;
				attachmentName.Format("MergedAttachment_%d", m_nMergedSkinCount++);

				pMergeTarget = new CAttachmentMerged(attachmentName, pAttachmentManager);
				CSkin* pSkin = new CSkin(attachmentName, 0);
				pSkin->m_arrModelMeshes.resize(context.nMaxLodCount);

				for (auto& mesh : pSkin->m_arrModelMeshes) // mark as resident so it cannot be streamed out
					++mesh.m_stream.nKeepResidentRefs;

				const uint32 loadingFlags = (pAttachmentManager->m_pSkelInstance->m_CharEditMode ? CA_CharEditModel : 0);
				g_pCharacterManager->RegisterModelSKIN(pSkin, loadingFlags);
				g_pCharacterManager->RegisterInstanceSkin(pSkin, pMergeTarget->m_pMergedSkinAttachment);
				pMergeTarget->m_pMergedSkinAttachment->m_pModelSkin = pSkin;

				arrMergedAttachments.push_back(pMergeTarget);
			}

			Merge(pMergeTarget, curAttachmentList, pAttachmentManager);
			m_nBytesAllocated += context.EstimateRequiredMemory();
		}
	}
}

void CAttachmentMerger::OnDeleteMergedAttachment(CAttachmentMerged* pAttachment)
{
	CRY_ASSERT(pAttachment != NULL);

	AttachmentRenderData renderData(pAttachment->m_pMergedSkinAttachment);
	uint nSize = MergeContext(renderData).EstimateRequiredMemory();
	CRY_ASSERT(nSize <= m_nBytesAllocated);

	m_nBytesAllocated -= nSize;
	m_bOutOfMemory = false;
}

inline Vec3 toVec3(const Vec3& v)    { return v; }
inline Vec3 toVec3(const Vec3f16& v) { return v.ToVec3(); }

template<typename VertexFormat, const bool RequiresTransform>
void CopyVertices_tpl(CAttachmentMerger::MeshStreams& dstStreams, uint dstVtxOffset, CAttachmentMerger::MeshStreams& srcStreams, uint numVertices, const Matrix34& transform)
{
	const uint8_t* __restrict pSrc = (uint8_t*) srcStreams.pPositions;
	const uint8_t* pSrcEnd = pSrc + numVertices * srcStreams.nPositionStride;

	uint8_t* __restrict pDst = (uint8_t*) dstStreams.pPositions + dstVtxOffset * dstStreams.nPositionStride;

	while (pSrc < pSrcEnd)
	{
		Vec3 v = toVec3(((VertexFormat*) pSrc)->xyz);

		if (RequiresTransform)
			v = transform * v;

		((SVF_P3F*) pDst)->xyz = v;

		pDst += dstStreams.nPositionStride;
		pSrc += srcStreams.nPositionStride;
	}
}

template<const bool RequiresTransform>
uint CopyVertices(CAttachmentMerger::MeshStreams& dstStreams, uint dstVtxOffset, CAttachmentMerger::MeshStreams& srcStreams, uint numVertices, const Matrix34& transform)
{
	InputLayoutHandle sVF = srcStreams.GetVertexFormat();

	if (sVF == EDefaultInputLayouts::P3S_C4B_T2S)
		CopyVertices_tpl<SVF_P3S_C4B_T2S, RequiresTransform>(dstStreams, dstVtxOffset, srcStreams, numVertices, transform);
	else if (sVF == EDefaultInputLayouts::P3F_C4B_T2F)
		CopyVertices_tpl<SVF_P3F_C4B_T2F, RequiresTransform>(dstStreams, dstVtxOffset, srcStreams, numVertices, transform);
	else if (sVF == EDefaultInputLayouts::P3F)
		CopyVertices_tpl<SVF_P3F, RequiresTransform>(dstStreams, dstVtxOffset, srcStreams, numVertices, transform);
	else
		CRY_ASSERT(false);

	return numVertices;
}

uint CAttachmentMerger::CopyVertices(MeshStreams& dstStreams, uint dstVtxOffset, MeshStreams& srcStreams, uint numVertices)
{
	return ::CopyVertices<false>(dstStreams, dstVtxOffset, srcStreams, numVertices, Matrix34(IDENTITY));
}
uint CAttachmentMerger::CopyVertices(MeshStreams& dstStreams, uint dstVtxOffset, MeshStreams& srcStreams, uint numVertices, const Matrix34& transform)
{
	return ::CopyVertices<true>(dstStreams, dstVtxOffset, srcStreams, numVertices, transform);
}

uint CAttachmentMerger::CopyIndices(MeshStreams& dstStreams, uint dstVtxOffset, uint dstIdxOffset, MeshStreams& srcStreams, const CRenderChunk& chunk)
{
#if defined(CRY_PLATFORM_SSE2)
	memcpy(dstStreams.pIndices + dstIdxOffset, srcStreams.pIndices + chunk.nFirstIndexId, chunk.nNumIndices * sizeof(vtx_idx));

	const bool b32BitIndices = sizeof(vtx_idx) == 4;
	const uint NumElementsSSE = b32BitIndices ? 4 : 8;

	__m128i dstIdxOffset4 = b32BitIndices ? _mm_set1_epi32(dstVtxOffset) : _mm_set1_epi16(dstVtxOffset);

	vtx_idx* pDstStart = dstStreams.pIndices + dstIdxOffset;
	vtx_idx* pDstEnd = dstStreams.pIndices + dstIdxOffset + chunk.nNumIndices;

	vtx_idx* pDstStartAligned = Align(pDstStart, 16);
	vtx_idx* pDstEndAligned = pDstStart + (chunk.nNumIndices & ~(NumElementsSSE - 1));

	vtx_idx* __restrict pCur = pDstStart;

	while (pCur < pDstStartAligned)
		*pCur++ += dstVtxOffset;

	for (; pCur < pDstEndAligned; pCur += NumElementsSSE)
	{
		__m128i v = _mm_load_si128((__m128i*)pCur);
		__m128i w = b32BitIndices ? _mm_add_epi32(v, dstIdxOffset4) : _mm_add_epi16(v, dstIdxOffset4);
		_mm_storeu_si128((__m128i*)pCur, w);
	}

	while (pCur < pDstEnd)
		*pCur++ += dstVtxOffset;

#else
	for (uint k = 0; k < chunk.nNumIndices; ++k)
	{
		vtx_idx idxSrc = srcStreams.GetVertexIndex(chunk.nFirstIndexId + k);
		vtx_idx& idxDst = dstStreams.GetVertexIndex(dstIdxOffset + k);
		idxDst = idxSrc + dstVtxOffset;
	}
#endif
	return chunk.nNumIndices;
}

uint CAttachmentMerger::CopySkinning(MeshStreams& dstStreams, uint dstVtxOffset, MeshStreams& srcStreams, uint numVertices, const DynArray<JointIdType>& boneIDs)
{
	CRY_ASSERT(srcStreams.pSkinningInfos);

	uint maxBoneIndex = 0;

	for (uint k = 0; k < numVertices; ++k)
	{
		const SVF_W4B_I4S& srcSkinningInfo = srcStreams.GetSkinningInfo(k);

		JointIdType id0 = srcSkinningInfo.indices[0];
		JointIdType id1 = srcSkinningInfo.indices[1];
		JointIdType id2 = srcSkinningInfo.indices[2];
		JointIdType id3 = srcSkinningInfo.indices[3];

		id0 = srcSkinningInfo.weights.bcolor[0] ? boneIDs[id0] : 0;
		id1 = srcSkinningInfo.weights.bcolor[1] ? boneIDs[id1] : 0;
		id2 = srcSkinningInfo.weights.bcolor[2] ? boneIDs[id2] : 0;
		id3 = srcSkinningInfo.weights.bcolor[3] ? boneIDs[id3] : 0;

		maxBoneIndex = max(maxBoneIndex, (uint)id0);
		maxBoneIndex = max(maxBoneIndex, (uint)id1);
		maxBoneIndex = max(maxBoneIndex, (uint)id2);
		maxBoneIndex = max(maxBoneIndex, (uint)id3);

		CRY_ASSERT(maxBoneIndex <= std::numeric_limits<JointIdType>::max());

		SVF_W4B_I4S& dstSkinningInfo = dstStreams.GetSkinningInfo(k + dstVtxOffset);

		dstSkinningInfo.weights = srcSkinningInfo.weights;
		dstSkinningInfo.indices[0] = id0;
		dstSkinningInfo.indices[1] = id1;
		dstSkinningInfo.indices[2] = id2;
		dstSkinningInfo.indices[3] = id3;
	}

	return maxBoneIndex;
}

void CAttachmentMerger::SkinToBone(MeshStreams& dstStreams, uint dstVtxOffset, MeshStreams& srcStreams, uint numVertices, JointIdType jointID)
{
	CRY_ASSERT(dstStreams.pSkinningInfos);

	for (uint k = 0; k < numVertices; ++k)
	{
		SVF_W4B_I4S& dstSkinningInfo = dstStreams.GetSkinningInfo(k + dstVtxOffset);

		dstSkinningInfo.weights.bcolor[0] = 255;
		dstSkinningInfo.weights.bcolor[1] = 0;
		dstSkinningInfo.weights.bcolor[2] = 0;
		dstSkinningInfo.weights.bcolor[3] = 0;
		dstSkinningInfo.indices[0] = jointID;
		dstSkinningInfo.indices[1] = 0;
		dstSkinningInfo.indices[2] = 0;
		dstSkinningInfo.indices[3] = 0;
	}
}

void CAttachmentMerger::Merge(CAttachmentMerged* pDstAttachment, const DynArray<AttachmentRenderData>& attachmentData, CAttachmentManager* pAttachmentManager)
{
	CSkin* pDstSkin = static_cast<CSkin*>(pDstAttachment->m_pMergedSkinAttachment->m_pModelSkin);
	CRY_ASSERT(pDstSkin->m_arrModelMeshes.size() > 0);

	DynArray<CSkin::SJointInfo> arrModelJoints = pDstSkin->m_arrModelJoints;
	const uint maxLODCount = pDstSkin->GetNumLODs();

	CCharInstance* pInstanceSkel = pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pDefaultSkeleton = pInstanceSkel->m_pDefaultSkeleton;

	DynArray<CAttachmentMerged::MergeInfo> mergeInfo;
	mergeInfo.resize(attachmentData.size());
	for (int i = 0; i < attachmentData.size(); ++i)
	{
		mergeInfo[i].pAttachment = attachmentData[i].pAttachment;
		mergeInfo[i].pAttachmentObject = attachmentData[i].pAttachment->GetIAttachmentObject();
		memset(&mergeInfo[i].IndexInfo, 0x0, sizeof(mergeInfo[i].IndexInfo));
	}

	for (uint lod = 0; lod < maxLODCount; ++lod)
	{
		CModelMesh* pModelMesh = pDstSkin->GetModelMesh(lod);
		IRenderMesh* pPrevRenderMesh = pDstSkin->GetIRenderMesh(lod);

		// Create rendermesh
		uint numVertices = pPrevRenderMesh ? pPrevRenderMesh->GetVerticesCount() : 0;
		uint numIndices = pPrevRenderMesh ? pPrevRenderMesh->GetIndicesCount() : 0;

		for (auto const& attData : attachmentData)
		{
			const AttachmentLodData& lodData = attData.GetLod(lod);

			for (auto const& chunk : lodData.GetShadowChunks())
				numIndices += chunk.nNumIndices;

			numVertices += lodData.pMesh->GetVerticesCount();
		}

		IRenderMesh::SInitParamerers params;
		params.nVertexCount = numVertices;
		params.nIndexCount = numIndices;
		params.eVertexFormat = TargetVertexFormat;

		_smart_ptr<IRenderMesh> pRenderMesh = g_pISystem->GetIRenderer()->CreateRenderMesh("MergedCharacter", pDstAttachment->m_pMergedSkinAttachment->GetName(), &params);

		// Copy data from attachments
		{
			MeshStreams dstStreams(pRenderMesh, FSL_SYSTEM_CREATE);
			CRY_ASSERT(dstStreams.pSkinningInfos);

			uint vertexBase = 0;
			uint indexBase = 0;
			uint maxBoneIndex = 0;

			// copy previous rendermesh data first
			if (pPrevRenderMesh)
			{
				MeshStreams srcStreams(pPrevRenderMesh);
				memcpy(dstStreams.pPositions, srcStreams.pPositions, pPrevRenderMesh->GetVerticesCount() * srcStreams.nPositionStride);
				memcpy(dstStreams.pSkinningInfos, srcStreams.pSkinningInfos, pPrevRenderMesh->GetVerticesCount() * srcStreams.nSkinningInfoStride);
				memcpy(dstStreams.pIndices, srcStreams.pIndices, pPrevRenderMesh->GetIndicesCount() * sizeof(srcStreams.pIndices[0]));

				vertexBase = pPrevRenderMesh->GetVerticesCount();
				indexBase = pPrevRenderMesh->GetIndicesCount();
				maxBoneIndex = pDstSkin->m_arrModelJoints.size();
			}

			for (int i = 0; i < attachmentData.size(); ++i)
			{
				const AttachmentRenderData& curAttachmentData = attachmentData[i];
				const AttachmentLodData& lodData = curAttachmentData.GetLod(lod);

				CAttachmentMerged::MergeInfo& curMergeInfo = mergeInfo[i];
				curMergeInfo.IndexInfo[lod].nFirstIndex = indexBase;

				IAttachmentObject* pAttachmentObject = curAttachmentData.pAttachment->GetIAttachmentObject();

				if (CAttachmentSKIN* pAttachmentSkin = static_cast<CAttachmentSKIN*>(pAttachmentObject->GetIAttachmentSkin())) // SKIN ATTACHMENT
				{
					CRY_ASSERT(lodData.pMesh != NULL);

					DynArray<CSkin::SJointInfo>& srcJoints = pAttachmentSkin->m_pModelSkin->m_arrModelJoints;
					DynArray<JointIdType> arrChunkBoneIDs;
					arrChunkBoneIDs.resize(srcJoints.size());

					// add bones to merged bones list and build remapping table
					for (uint k = 0; k < srcJoints.size(); ++k)
					{
						auto compareToCurrentJoint = [&](const CSkin::SJointInfo& j0) { return j0.m_nJointCRC32Lower == srcJoints[k].m_nJointCRC32Lower; };
						DynArray<CSkin::SJointInfo>::const_iterator it = std::find_if(arrModelJoints.begin(), arrModelJoints.end(), compareToCurrentJoint);

						if (it == arrModelJoints.end())
							arrModelJoints.push_back(srcJoints[k]);

						arrChunkBoneIDs[k] = pDefaultSkeleton->GetJointIDByCRC32(srcJoints[k].m_nJointCRC32Lower);
					}

					MeshStreams srcStreams(lodData.pMesh);
					uint vertexCount = CopyVertices(dstStreams, vertexBase, srcStreams, lodData.pMesh->GetVerticesCount(), lodData.mTransform);
					uint maxBoneID = CopySkinning(dstStreams, vertexBase, srcStreams, lodData.pMesh->GetVerticesCount(), arrChunkBoneIDs);

					for (auto const& chunk : lodData.GetShadowChunks())
						indexBase += CopyIndices(dstStreams, vertexBase, indexBase, srcStreams, chunk);

					vertexBase += lodData.pMesh->GetVerticesCount();
					maxBoneIndex = max(maxBoneIndex, maxBoneID);
				}
				else // BONE/FACE ATTACHMENT
				{
					CRY_ASSERT(lod != 0 || pAttachmentManager->FindExtraBone(curAttachmentData.pAttachment) == -1);

					JointIdType jointID = pAttachmentManager->FindExtraBone(curAttachmentData.pAttachment);
					if (jointID == (JointIdType) - 1)
					{
						jointID = pAttachmentManager->AddExtraBone(curAttachmentData.pAttachment);

						QuatT jointDefaultAbsolute(IDENTITY);
						JointIdType idAttachedTo = curAttachmentData.pAttachment->GetJointID();

						if (idAttachedTo < pAttachmentManager->GetSkelInstance()->GetIDefaultSkeleton().GetJointCount())
							jointDefaultAbsolute = pAttachmentManager->GetSkelInstance()->GetIDefaultSkeleton().GetDefaultAbsJointByID(idAttachedTo);

						CSkin::SJointInfo jointInfo;
						jointInfo.m_DefaultAbsolute = jointDefaultAbsolute;
						jointInfo.m_DefaultAbsolute.q.Normalize();
						jointInfo.m_idxParent = -1;
						jointInfo.m_nJointCRC32Lower = -1;
						jointInfo.m_nExtraJointID = jointID;
						jointInfo.m_NameModelSkin = pDstAttachment->m_pMergedSkinAttachment->GetName();
						arrModelJoints.push_back(jointInfo);
					}

					jointID += pDefaultSkeleton->m_arrModelJoints.size();

					maxBoneIndex = max(maxBoneIndex, (uint)jointID);
					CRY_ASSERT(maxBoneIndex <= std::numeric_limits<JointIdType>::max());

					MeshStreams srcStreams(lodData.pMesh, FSL_READ, false);
					uint vertexCount = CopyVertices(dstStreams, vertexBase, srcStreams, lodData.pMesh->GetVerticesCount());
					SkinToBone(dstStreams, vertexBase, srcStreams, lodData.pMesh->GetVerticesCount(), jointID);

					for (auto const& chunk : lodData.GetShadowChunks())
						indexBase += CopyIndices(dstStreams, vertexBase, indexBase, srcStreams, chunk);

					vertexBase += lodData.pMesh->GetVerticesCount();
				}

				curMergeInfo.IndexInfo[lod].nIndexCount = indexBase - curMergeInfo.IndexInfo[lod].nFirstIndex;
			}

			numIndices = indexBase;
			pDstAttachment->m_MergedAttachmentIndices[lod].assign(dstStreams.pIndices, dstStreams.pIndices + numIndices);

			CRY_ASSERT(vertexBase == numVertices);
			CRY_ASSERT(indexBase == numIndices);
			CRY_ASSERT(maxBoneIndex < std::numeric_limits<JointIdType>::max());
		}

		auto const& firstAttachmentLod = attachmentData[0].GetLod(lod);
		auto firstAttachmentChunk = firstAttachmentLod.GetShadowChunks().begin();

		pModelMesh->m_pIRenderMesh = pRenderMesh;
		pModelMesh->m_pIDefaultMaterial = firstAttachmentLod.pMaterial->GetSafeSubMtl(firstAttachmentChunk->m_nMatID);
		pModelMesh->m_arrRenderChunks.resize(1);
		pModelMesh->m_arrRenderChunks[0].m_nFirstIndexId = 0;
		pModelMesh->m_arrRenderChunks[0].m_nNumIndices = numIndices;

		CRenderChunk chunk;
		chunk.m_nMatID = 0;
		chunk.nNumVerts = numVertices;
		chunk.nNumIndices = numIndices;

		if (chunk.nNumVerts > 0 && chunk.nNumIndices > 0)
		{
			pRenderMesh->SetChunk(-1, chunk);
			pRenderMesh->GetChunks()[0].pRE->mfUpdateFlags(FCEF_SKINNED);
			pRenderMesh->SetSkinned();
		}
	}

	pDstSkin->m_arrModelJoints = arrModelJoints;

	CSKINAttachment* pSkinInstance = new CSKINAttachment();
	pSkinInstance->m_pIAttachmentSkin = pDstAttachment->m_pMergedSkinAttachment;
	pDstAttachment->AddAttachments(mergeInfo);
	pDstAttachment->m_pMergedSkinAttachment->Immediate_AddBinding(pSkinInstance, pDstSkin, CA_SkipBoneRemapping);
}

void CAttachmentMerger::UpdateIndices(CAttachmentMerged* pAttachment, int lod, const DynArray<std::pair<uint32, uint32>>& srcRanges)
{
	CRY_ASSERT(pAttachment && pAttachment->m_pMergedSkinAttachment->m_pModelSkin);
	CRY_ASSERT(lod >= 0 && lod <= MAX_STATOBJ_LODS_NUM);

	DEFINE_PROFILER_FUNCTION();

	uint indexCount = 0;
	for (auto& curRange : srcRanges)
		indexCount += curRange.second;

	DynArray<vtx_idx> newIndices;
	newIndices.resize(indexCount);

	uint dstOffset = 0;
	for (auto& curRange : srcRanges)
	{
		if (curRange.second > 0)
		{
			memcpy(&newIndices[dstOffset], &pAttachment->m_MergedAttachmentIndices[lod][curRange.first], curRange.second * sizeof(vtx_idx));
			dstOffset += curRange.second;
		}
	}

	IRenderMesh* pDstMesh = pAttachment->m_pMergedSkinAttachment->m_pModelSkin->GetIRenderMesh(lod);
	if (pDstMesh)
	{
		pDstMesh->LockForThreadAccess();
		pDstMesh->UpdateIndices(indexCount > 0 ? &newIndices[0] : NULL, newIndices.size(), 0, 0);
		pDstMesh->UnLockForThreadAccess();
	}

}
