// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "KDTree.h"

class KDTreeNode
{
public:
	KDTreeNode()
	{
		pChildren[0] = NULL;
		pChildren[1] = NULL;
		pVertexIndices = NULL;
	}
	~KDTreeNode()
	{
		if (!IsLeaf())
		{
			if (pChildren[0])
				delete pChildren[0];
			if (pChildren[1])
				delete pChildren[1];
		}
		else if (GetVertexBufferSize() > 1)
		{
			if (pVertexIndices)
				delete[] pVertexIndices;
		}
	}
	uint32 GetVertexBufferSize() const
	{
		return nVertexIndexBufferSize;
	}
	float GetSplitPos() const
	{
		return splitPos;
	}
	void SetSplitPos(float pos)
	{
		splitPos = pos;
	}
	CKDTree::ESplitAxis GetSplitAxis() const
	{
		if (splitAxis == 0)  return CKDTree::eSA_X;
		if (splitAxis == 1)  return CKDTree::eSA_Y;
		if (splitAxis == 2)  return CKDTree::eSA_Z;
		return CKDTree::eSA_Invalid;
	}
	void SetSplitAxis(const CKDTree::ESplitAxis& axis)
	{
		splitAxis = axis;
	}
	bool IsLeaf() const
	{
		return pChildren[0] == NULL && pChildren[1] == NULL;
	}
	KDTreeNode* GetChild(uint32 nIndex) const
	{
		if (nIndex > 1)
			return NULL;
		return pChildren[nIndex];
	}
	void SetChild(uint32 nIndex, KDTreeNode* pNode)
	{
		if (nIndex > 1)
			return;
		if (pChildren[nIndex])
			delete pChildren[nIndex];
		pChildren[nIndex] = pNode;
	}
	const AABB& GetBoundBox()
	{
		return boundbox;
	}
	void SetBoundBox(const AABB& aabb)
	{
		boundbox = aabb;
	}
	void SetVertexIndexBuffer(std::vector<uint32>& vertexInfos)
	{
		nVertexIndexBufferSize = (uint32)vertexInfos.size();
		if (nVertexIndexBufferSize == 0)
			return;
		if (nVertexIndexBufferSize == 1)
		{
			oneIndex = vertexInfos[0];
		}
		else
		{
			pVertexIndices = new uint32[nVertexIndexBufferSize];
			memcpy(pVertexIndices, &vertexInfos[0], sizeof(uint32) * nVertexIndexBufferSize);
		}
	}
	uint32 GetVertexIndex(uint32 nIndex) const
	{
		if (GetVertexBufferSize() == 1)
			return oneIndex & 0x00FFFFFF;

		return pVertexIndices[nIndex] & 0x00FFFFFF;
	}
	uint32 GetObjIndex(uint32 nIndex) const
	{
		if (GetVertexBufferSize() == 1)
			return (oneIndex & 0xFF000000) >> 24;

		return (pVertexIndices[nIndex] & 0xFF000000) >> 24;
	}

private:
	union
	{
		float   splitPos;       // Interior
		uint32  oneIndex;       // Leaf
		uint32* pVertexIndices; // Leaf : high 8bits - object index, low 24bits - vertex index
	};
	union
	{
		uint32 splitAxis;              // Interior
		uint32 nVertexIndexBufferSize; // Leaf
	};
	AABB        boundbox;     // Both
	KDTreeNode* pChildren[2]; // Interior
};

CKDTree::ESplitAxis SearchForBestSplitAxis(const AABB& aabb)
{
	float xsize = aabb.max.x - aabb.min.x;
	float ysize = aabb.max.y - aabb.min.y;
	float zsize = aabb.max.z - aabb.min.z;

	CKDTree::ESplitAxis axis;
	if (xsize > ysize && xsize > zsize)
		axis = CKDTree::eSA_X;
	else if (ysize > zsize && ysize > xsize)
		axis = CKDTree::eSA_Y;
	else
		axis = CKDTree::eSA_Z;

	return axis;
}

bool SearchForBestSplitPos(CKDTree::ESplitAxis axis, const std::vector<CKDTree::SStatObj>& statObjList, std::vector<uint32>& indices, float& outBestSplitPos)
{
	if (axis != CKDTree::eSA_X && axis != CKDTree::eSA_Y && axis != CKDTree::eSA_Z)
		return false;

	outBestSplitPos = 0;

	int nSizeOfIndices(indices.size());

	for (int i = 0; i < nSizeOfIndices; ++i)
	{
		int nObjIndex = (indices[i] & 0xFF000000) >> 24;
		int nVertexIndex = (indices[i] & 0xFFFFFF);

		const CKDTree::SStatObj* pObj = &statObjList[nObjIndex];

		const IIndexedMesh* pMesh = pObj->pStatObj->GetIndexedMesh();
		if (pMesh == NULL)
			continue;

		IIndexedMesh::SMeshDescription meshDesc;
		pMesh->GetMeshDescription(meshDesc);

		if (meshDesc.m_pVerts)
			outBestSplitPos += pObj->tm.TransformPoint(meshDesc.m_pVerts[nVertexIndex])[axis];
		else if (meshDesc.m_pVertsF16)
			outBestSplitPos += pObj->tm.TransformPoint(meshDesc.m_pVertsF16[nVertexIndex].ToVec3())[axis];
	}

	outBestSplitPos /= nSizeOfIndices;

	return true;
}

struct SSplitInfo
{
	AABB                aboveBoundbox;
	std::vector<uint32> aboveIndices;
	AABB                belowBoundbox;
	std::vector<uint32> belowIndices;
};

bool SplitNode(const std::vector<CKDTree::SStatObj>& statObjList, const AABB& boundbox, const std::vector<uint32>& indices, CKDTree::ESplitAxis splitAxis, float splitPos, SSplitInfo& outInfo)
{
	if (splitAxis != CKDTree::eSA_X && splitAxis != CKDTree::eSA_Y && splitAxis != CKDTree::eSA_Z)
		return false;

	outInfo.aboveBoundbox = boundbox;
	outInfo.belowBoundbox = boundbox;

	outInfo.aboveBoundbox.max[splitAxis] = splitPos;
	outInfo.belowBoundbox.min[splitAxis] = splitPos;

	uint32 iIndexSize = (uint32)indices.size();
	outInfo.aboveIndices.reserve(iIndexSize);
	outInfo.belowIndices.reserve(iIndexSize);

	for (uint32 i = 0; i < iIndexSize; ++i)
	{
		int nObjIndex = (indices[i] & 0xFF000000) >> 24;
		int nVertexIndex = indices[i] & 0xFFFFFF;

		const CKDTree::SStatObj* pObj = &statObjList[nObjIndex];

		const IIndexedMesh* pMesh = pObj->pStatObj->GetIndexedMesh();
		if (pMesh == NULL)
			return false;

		IIndexedMesh::SMeshDescription meshDesc;
		pMesh->GetMeshDescription(meshDesc);

		Vec3 vPos;
		if (meshDesc.m_pVerts)
			vPos = pObj->tm.TransformPoint(meshDesc.m_pVerts[nVertexIndex]);
		else if (meshDesc.m_pVertsF16)
			vPos = pObj->tm.TransformPoint(meshDesc.m_pVertsF16[nVertexIndex].ToVec3());
		else
			continue;

		if (vPos[splitAxis] < splitPos)
		{
			outInfo.aboveIndices.push_back(indices[i]);
			assert(outInfo.aboveBoundbox.IsContainPoint(vPos));
		}
		else
		{
			outInfo.belowIndices.push_back(indices[i]);
			assert(outInfo.belowBoundbox.IsContainPoint(vPos));
		}
	}

	return true;
}

CKDTree::CKDTree()
{
	m_pRootNode = NULL;
}

CKDTree::~CKDTree()
{
	if (m_pRootNode)
		delete m_pRootNode;
}

bool CKDTree::Build(IStatObj* pStatObj)
{
	if (pStatObj == NULL)
		return false;

	m_StatObjectList.clear();

	if (pStatObj->GetIndexedMesh(true))
	{
		SStatObj rootObj;
		rootObj.tm.SetIdentity();
		rootObj.pStatObj = pStatObj;
		m_StatObjectList.push_back(rootObj);
	}

	ConstructStatObjList(pStatObj, Matrix34::CreateIdentity());

	AABB entireBoundBox;
	entireBoundBox.Reset();

	std::vector<uint32> indices;
	for (int i = 0, iStatObjSize(m_StatObjectList.size()); i < iStatObjSize; ++i)
	{
		IIndexedMesh* pMesh = m_StatObjectList[i].pStatObj->GetIndexedMesh(true);
		if (pMesh == NULL)
			continue;

		IIndexedMesh::SMeshDescription meshDesc;
		pMesh->GetMeshDescription(meshDesc);

		for (int k = 0; k < meshDesc.m_nVertCount; ++k)
		{
			entireBoundBox.Add(m_StatObjectList[i].tm.TransformPoint(meshDesc.m_pVerts[k]));
			indices.push_back((i << 24) | k);
		}
	}

	if (m_pRootNode)
		delete m_pRootNode;

	m_pRootNode = new KDTreeNode;
	BuildRecursively(m_pRootNode, entireBoundBox, indices);

	return true;
}

void CKDTree::BuildRecursively(KDTreeNode* pNode, const AABB& boundbox, std::vector<uint32>& indices) const
{
	pNode->SetBoundBox(boundbox);

	if (indices.size() <= s_MinimumVertexSizeInLeafNode)
	{
		pNode->SetVertexIndexBuffer(indices);
		return;
	}

	ESplitAxis splitAxis = SearchForBestSplitAxis(boundbox);
	float splitPos(0);
	SearchForBestSplitPos(splitAxis, m_StatObjectList, indices, splitPos);
	pNode->SetSplitAxis(splitAxis);
	pNode->SetSplitPos(splitPos);

	SSplitInfo splitInfo;
	if (!SplitNode(m_StatObjectList, boundbox, indices, splitAxis, splitPos, splitInfo))
		return;

	if (splitInfo.aboveIndices.empty() || splitInfo.belowIndices.empty())
	{
		pNode->SetVertexIndexBuffer(indices);
		return;
	}

	KDTreeNode* pChild0 = new KDTreeNode;
	KDTreeNode* pChild1 = new KDTreeNode;

	pNode->SetChild(0, pChild0);
	pNode->SetChild(1, pChild1);

	BuildRecursively(pChild0, splitInfo.aboveBoundbox, splitInfo.aboveIndices);
	BuildRecursively(pChild1, splitInfo.belowBoundbox, splitInfo.belowIndices);
}

void CKDTree::ConstructStatObjList(IStatObj* pStatObj, const Matrix34& matParent)
{
	if (pStatObj == NULL)
		return;
	for (int i = 0, nChildObjSize(pStatObj->GetSubObjectCount()); i < nChildObjSize; ++i)
	{
		IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(i);
		SStatObj s;
		s.tm = matParent * pSubObj->localTM;
		if (pSubObj->pStatObj && pSubObj->pStatObj->GetIndexedMesh(true))
		{
			s.pStatObj = pSubObj->pStatObj;
			m_StatObjectList.push_back(s);
		}
		ConstructStatObjList(pSubObj->pStatObj, s.tm);
	}
}

bool CKDTree::FindNearestVertex(const Vec3& raySrc, const Vec3& rayDir, float vVertexBoxSize, const Vec3& localCameraPos, Vec3& outPos, Vec3& vOutHitPosOnCube) const
{
	return FindNearestVertexRecursively(m_pRootNode, raySrc, rayDir, vVertexBoxSize, localCameraPos, outPos, vOutHitPosOnCube);
}

AABB GetNodeBoundBox(KDTreeNode* pNode, float vVertexBoxSize, const Vec3& localCameraPos)
{
	AABB nodeAABB = pNode->GetBoundBox();
	float fScreenFactorMin = localCameraPos.GetDistance(nodeAABB.min);
	Vec3 vBoundBoxMin(fScreenFactorMin * vVertexBoxSize, fScreenFactorMin * vVertexBoxSize, fScreenFactorMin * vVertexBoxSize);
	float fScreenFactorMax = localCameraPos.GetDistance(nodeAABB.max);
	Vec3 vBoundBoxMax(fScreenFactorMax * vVertexBoxSize, fScreenFactorMax * vVertexBoxSize, fScreenFactorMax * vVertexBoxSize);
	nodeAABB.min -= vBoundBoxMin;
	nodeAABB.max += vBoundBoxMax;
	return nodeAABB;
}

bool CKDTree::FindNearestVertexRecursively(KDTreeNode* pNode, const Vec3& raySrc, const Vec3& rayDir, float vVertexBoxSize, const Vec3& localCameraPos, Vec3& outPos, Vec3& vOutHitPosOnCube) const
{
	if (!pNode)
		return false;

	Vec3 vHitPos;
	AABB nodeAABB = GetNodeBoundBox(pNode, vVertexBoxSize, localCameraPos);
	if (!pNode->GetBoundBox().IsContainPoint(raySrc) && !Intersect::Ray_AABB(raySrc, rayDir, nodeAABB, vHitPos))
		return false;

	if (pNode->IsLeaf())
	{
		if (m_StatObjectList.empty())
			return false;

		uint32 nVBuffSize = pNode->GetVertexBufferSize();
		if (nVBuffSize == 0)
			return false;

		float fNearestDist = 3e10f;

		for (uint32 i = 0; i < nVBuffSize; ++i)
		{
			uint32 nVertexIndex = pNode->GetVertexIndex(i);
			uint32 nObjIndex = pNode->GetObjIndex(i);

			assert(nObjIndex < m_StatObjectList.size() && nObjIndex >= 0);

			const SStatObj* pStatObjInfo = &(m_StatObjectList[nObjIndex]);

			IIndexedMesh* pMesh = m_StatObjectList[nObjIndex].pStatObj->GetIndexedMesh();
			if (pMesh == NULL)
				continue;

			IIndexedMesh::SMeshDescription meshDesc;
			pMesh->GetMeshDescription(meshDesc);

			Vec3 vCandidatePos(0, 0, 0);
			if (meshDesc.m_pVerts)
				vCandidatePos = pStatObjInfo->tm.TransformPoint(meshDesc.m_pVerts[nVertexIndex]);
			else if (meshDesc.m_pVertsF16)
				vCandidatePos = pStatObjInfo->tm.TransformPoint(meshDesc.m_pVertsF16[nVertexIndex].ToVec3());
			else
				continue;

			float fScreenFactor = localCameraPos.GetDistance(vCandidatePos);
			Vec3 vBoundBox(fScreenFactor * vVertexBoxSize, fScreenFactor * vVertexBoxSize, fScreenFactor * vVertexBoxSize);

			Vec3 vHitPosOnCube;
			if (Intersect::Ray_AABB(raySrc, rayDir, AABB(vCandidatePos - vBoundBox, vCandidatePos + vBoundBox), vHitPosOnCube))
			{
				float fDist = vHitPosOnCube.GetDistance(raySrc);
				if (fDist < fNearestDist)
				{
					fNearestDist = fDist;
					outPos = vCandidatePos;
					vOutHitPosOnCube = vHitPosOnCube;
				}
			}
		}

		if (fNearestDist < 3e10f)
			return true;

		return false;
	}

	Vec3 vNearestPos0, vNearestPos0OnCube;
	Vec3 vNearestPos1, vNearestPos1OnCube;
	bool bFoundChild0 = FindNearestVertexRecursively(pNode->GetChild(0), raySrc, rayDir, vVertexBoxSize, localCameraPos, vNearestPos0, vNearestPos0OnCube);
	bool bFoundChild1 = FindNearestVertexRecursively(pNode->GetChild(1), raySrc, rayDir, vVertexBoxSize, localCameraPos, vNearestPos1, vNearestPos1OnCube);

	if (bFoundChild0 && bFoundChild1)
	{
		float fDist0 = raySrc.GetDistance(vNearestPos0OnCube);
		float fDist1 = raySrc.GetDistance(vNearestPos1OnCube);
		if (fDist0 < fDist1)
		{
			outPos = vNearestPos0;
			vOutHitPosOnCube = vNearestPos0OnCube;
		}
		else
		{
			outPos = vNearestPos1;
			vOutHitPosOnCube = vNearestPos1OnCube;
		}
	}
	else if (bFoundChild0 && !bFoundChild1)
	{
		outPos = vNearestPos0;
		vOutHitPosOnCube = vNearestPos0OnCube;
	}
	else if (!bFoundChild0 && bFoundChild1)
	{
		outPos = vNearestPos1;
		vOutHitPosOnCube = vNearestPos1OnCube;
	}

	return bFoundChild0 || bFoundChild1;
}

void CKDTree::GetPenetratedBoxes(const Vec3& raySrc, const Vec3& rayDir, std::vector<AABB>& outBoxes)
{
	GetPenetratedBoxesRecursively(m_pRootNode, raySrc, rayDir, outBoxes);
}

void CKDTree::GetPenetratedBoxesRecursively(KDTreeNode* pNode, const Vec3& raySrc, const Vec3& rayDir, std::vector<AABB>& outBoxes)
{
	Vec3 vHitPos;
	if (!pNode || (!pNode->GetBoundBox().IsContainPoint(raySrc) && !Intersect::Ray_AABB(raySrc, rayDir, pNode->GetBoundBox(), vHitPos)))
		return;

	outBoxes.push_back(pNode->GetBoundBox());

	GetPenetratedBoxesRecursively(pNode->GetChild(0), raySrc, rayDir, outBoxes);
	GetPenetratedBoxesRecursively(pNode->GetChild(1), raySrc, rayDir, outBoxes);
}

