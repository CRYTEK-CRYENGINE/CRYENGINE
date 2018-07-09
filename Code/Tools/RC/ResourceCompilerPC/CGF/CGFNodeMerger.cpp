// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CGFNodeMerger.h"
#include <Cry3DEngine/CGF/CGFContent.h>
#include "StringHelpers.h"

bool CGFNodeMerger::SetupMeshSubsets(const CContentCGF& contentCgf, CMesh& mesh, CMaterialCGF* pMaterialCGF, string& errorMessage)
{
	if (mesh.m_subsets.empty())
	{
		const DynArray<int>& usedMaterialIds = contentCgf.GetUsedMaterialIDs();
		for (int i = 0; i < usedMaterialIds.size(); ++i)
		{
			SMeshSubset meshSubset;
			meshSubset.nMatID = usedMaterialIds[i];
			meshSubset.nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			mesh.m_subsets.push_back(meshSubset);
		}
	}

	// Copy physicalization type from material to subsets (and fix subsets' matId if needed)
	if (pMaterialCGF)
	{
		for (int i = 0; i < mesh.m_subsets.size(); ++i)
		{
			SMeshSubset& meshSubset = mesh.m_subsets[i];
			if (pMaterialCGF->subMaterials.size() > 0)
			{
				int id = meshSubset.nMatID;
				if (id >= (int)pMaterialCGF->subMaterials.size())
				{
					// Let's use 3dsMax's approach of handling material ids out of range
					id %= (int)pMaterialCGF->subMaterials.size();
				}

				if (id >= 0 && pMaterialCGF->subMaterials[id] != nullptr)
				{
					meshSubset.nMatID = id;
					meshSubset.nPhysicalizeType = pMaterialCGF->subMaterials[id]->nPhysicalizeType;
				}
				else
				{
					errorMessage = StringHelpers::Format(
						"%s: Submaterial %d is not available for subset %d (%d subsets) in %s",
						__FUNCTION__, meshSubset.nMatID, i, (int)mesh.m_subsets.size(), contentCgf.GetFilename());
					return false;
				}
			}
			else
			{
				meshSubset.nPhysicalizeType = pMaterialCGF->nPhysicalizeType;
			}
		}
	}

	return true;
}


bool CGFNodeMerger::MergeNodes(const CContentCGF* pCGF, const std::vector<CNodeCGF*>& nodes, string& errorMessage, CMesh* pMergedMesh)
{
	assert(pMergedMesh);

	AABB meshBBox;
	meshBBox.Reset();

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const CNodeCGF* const pNode = nodes[i];
		assert(pNode->pMesh == 0 || pNode->pMesh->m_pPositionsF16 == 0);
		const int oldVertexCount = pMergedMesh->GetVertexCount();
		if (oldVertexCount == 0)
		{
			pMergedMesh->CopyFrom(*pNode->pMesh);
		}
		else
		{
			const char* const errText = pMergedMesh->AppendStreamsFrom(*pNode->pMesh);
			if (errText)
			{
				errorMessage = errText;
				return false;
			}
			// Keep color stream in sync size with vertex/normals stream.
			if (pMergedMesh->m_streamSize[CMesh::COLORS_0] > 0 && pMergedMesh->m_streamSize[CMesh::COLORS_0] < oldVertexCount)
			{
				const int count = pMergedMesh->m_streamSize[CMesh::COLORS_0];
				pMergedMesh->ReallocStream(CMesh::COLORS_0, oldVertexCount);
				memset(pMergedMesh->m_pColor0 + count, 255, (oldVertexCount - count) * sizeof(SMeshColor));
			}
			if (pMergedMesh->m_streamSize[CMesh::COLORS_1] > 0 && pMergedMesh->m_streamSize[CMesh::COLORS_1] < oldVertexCount)
			{
				const int count = pMergedMesh->m_streamSize[CMesh::COLORS_1];
				pMergedMesh->ReallocStream(CMesh::COLORS_1, oldVertexCount);
				memset(pMergedMesh->m_pColor1 + count, 255, (oldVertexCount - count) * sizeof(SMeshColor));
			}
		}

		AABB bbox = pNode->pMesh->m_bbox;
		if (!pNode->bIdentityMatrix)
		{
			bbox.SetTransformedAABB(pNode->worldTM, bbox);
		}

		meshBBox.Add(bbox.min);
		meshBBox.Add(bbox.max);
		pMergedMesh->m_bbox = meshBBox;

		if (!pNode->bIdentityMatrix)
		{
			// Transform merged mesh into the world space.
			// Only transform newly added vertices.
			for (int j = oldVertexCount; j < pMergedMesh->GetVertexCount(); ++j)
			{
				pMergedMesh->m_pPositions[j] = pNode->worldTM.TransformPoint(pMergedMesh->m_pPositions[j]);
				pMergedMesh->m_pNorms[j].RotateSafelyBy(pNode->worldTM);
			}
		}
	}

	if (pCGF)
	{
		if (!SetupMeshSubsets(*pCGF, *pMergedMesh, pCGF->GetCommonMaterial(), errorMessage))
		{
			return false;
		}
	}

	pMergedMesh->RecomputeTexMappingDensity();	

	return true;
}
