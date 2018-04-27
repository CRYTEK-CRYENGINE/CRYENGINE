// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "BSPTree3D.h"

void CBSPTree3D::BSPTreeNode::GetMemoryUsage(class ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "BSPTreeNode");
	pSizer->AddObject(this, sizeof(*this));
}

AreaUtil::EPointPosEnum CBSPTree3D::BSPTreeNode::IsPointIn(const Vec3& vPos, const NodeStorage& nodeStorage) const
{
	float distance(m_Plane.Distance(vPos));

	if (distance > AreaUtil::EPSILON)
	{
		if (m_PosChild != CBSPTree3D::kInvalidNodeIndex)
			return nodeStorage[m_PosChild].IsPointIn(vPos, nodeStorage);
		else
			return AreaUtil::ePP_OUTSIDE;
	}
	else if (distance < -AreaUtil::EPSILON)
	{
		if (m_NegChild != CBSPTree3D::kInvalidNodeIndex)
			return nodeStorage[m_NegChild].IsPointIn(vPos, nodeStorage);
		else
			return AreaUtil::ePP_INSIDE;
	}
	else
	{
		if (m_PosChild != CBSPTree3D::kInvalidNodeIndex || m_NegChild != CBSPTree3D::kInvalidNodeIndex)
		{
			AreaUtil::EPointPosEnum posResult = (m_PosChild != CBSPTree3D::kInvalidNodeIndex) ? nodeStorage[m_PosChild].IsPointIn(vPos, nodeStorage) : AreaUtil::ePP_OUTSIDE;
			AreaUtil::EPointPosEnum negResult = (m_NegChild != CBSPTree3D::kInvalidNodeIndex) ? nodeStorage[m_NegChild].IsPointIn(vPos, nodeStorage) : AreaUtil::ePP_OUTSIDE;

			if (posResult == AreaUtil::ePP_INSIDE || negResult == AreaUtil::ePP_INSIDE)
			{
				return AreaUtil::ePP_INSIDE;
			}
			else if (posResult == AreaUtil::ePP_BORDER || negResult == AreaUtil::ePP_BORDER)
			{
				return AreaUtil::ePP_BORDER;
			}
			return AreaUtil::ePP_OUTSIDE;
		}
		return AreaUtil::ePP_BORDER;
	}
}

CBSPTree3D::CBSPTree3D(const IBSPTree3D::FaceList& faceList)
{
	if (!faceList.empty())
	{
		BuildTree(faceList, m_BSPTree);
	}
}

bool CBSPTree3D::IsInside(const Vec3& vPos) const
{
	if (m_BSPTree.empty())
		return false;

	return m_BSPTree[0].IsPointIn(vPos, m_BSPTree) == AreaUtil::ePP_INSIDE;
}

void CBSPTree3D::BuildTree(const IBSPTree3D::FaceList& faceList, NodeStorage& treeNodes) const
{
	std::queue<GenerateNodeTask> taskQueue;
	taskQueue.push(GenerateNodeTask());
	taskQueue.back().m_FaceList = faceList;
	taskQueue.back().m_TargetNode = 0;
	treeNodes.resize(1);

	while (!taskQueue.empty())
	{
		const GenerateNodeTask& curTask = taskQueue.front();
		if (!curTask.m_FaceList.empty())
		{
			// create tasks for pos and neg nodes
			taskQueue.push(GenerateNodeTask());
			GenerateNodeTask& posNodeTask = taskQueue.back();
			taskQueue.push(GenerateNodeTask());
			GenerateNodeTask& negNodeTask = taskQueue.back();

			AreaUtil::CPlaneBase splitPlane(curTask.m_FaceList[0][0], curTask.m_FaceList[0][1], curTask.m_FaceList[0][2]);

			for (int i = 1, faceSize(curTask.m_FaceList.size()); i < faceSize; ++i)
			{
				IBSPTree3D::CFace posFace;
				IBSPTree3D::CFace negFace;

				AreaUtil::ESplitResult type = SplitFaceByPlane(splitPlane, curTask.m_FaceList[i], posFace, negFace);

				if (type == AreaUtil::eSR_CROSS)
				{
					posNodeTask.m_FaceList.push_back(posFace);
					negNodeTask.m_FaceList.push_back(negFace);
				}
				else if (type == AreaUtil::eSR_POSITIVE)
				{
					posNodeTask.m_FaceList.push_back(curTask.m_FaceList[i]);
				}
				else if (type == AreaUtil::eSR_NEGATIVE)
				{
					negNodeTask.m_FaceList.push_back(curTask.m_FaceList[i]);
				}
			}

			treeNodes[curTask.m_TargetNode].m_Plane = splitPlane;
			treeNodes[curTask.m_TargetNode].m_PosChild = kInvalidNodeIndex;
			treeNodes[curTask.m_TargetNode].m_NegChild = kInvalidNodeIndex;

			if (!posNodeTask.m_FaceList.empty())
			{
				treeNodes[curTask.m_TargetNode].m_PosChild = treeNodes.size();
				treeNodes.push_back(BSPTreeNode());

				posNodeTask.m_TargetNode = treeNodes[curTask.m_TargetNode].m_PosChild;
			}

			if (!negNodeTask.m_FaceList.empty())
			{
				treeNodes[curTask.m_TargetNode].m_NegChild = treeNodes.size();
				treeNodes.push_back(BSPTreeNode());

				negNodeTask.m_TargetNode = treeNodes[curTask.m_TargetNode].m_NegChild;
			}
		}

		taskQueue.pop();
	}
}

AreaUtil::ESplitResult CBSPTree3D::SplitFaceByPlane(const AreaUtil::CPlaneBase& plane, const IBSPTree3D::CFace& inFace, IBSPTree3D::CFace& outPosFace, IBSPTree3D::CFace& outNegFace)
{
	std::vector<char> signInfoList;
	signInfoList.resize(inFace.size());
	bool bPosPt = false;
	bool bNegPt = false;

	for (int i = 0, iPtSize(inFace.size()); i < iPtSize; ++i)
	{
		const Vec3& p(inFace[i]);
		float d(plane.Distance(p));
		if (d < -AreaUtil::EPSILON)
		{
			bNegPt = true;
			signInfoList[i] = -1;
		}
		else if (d > AreaUtil::EPSILON)
		{
			bPosPt = true;
			signInfoList[i] = 1;
		}
		else
		{
			signInfoList[i] = 0;
		}
	}

	if (bPosPt && bNegPt)
	{
		int nIndexFromPlusToMinus = -1;
		int nIndexFromMinusToPlus = -1;
		Vec3 vFromPlusToMinus;
		Vec3 vFromMinusToPlus;

		for (int i = 0, iPtSize(signInfoList.size()); i < iPtSize; )
		{
			int next_i = i;
			while (signInfoList[(++next_i) % iPtSize] == 0)
				;
			int i2(next_i);
			next_i %= iPtSize;
			if (signInfoList[i] == 1 && signInfoList[next_i] == -1)
			{
				nIndexFromPlusToMinus = next_i;
				float t(0);
				if (i2 - i == 1)
					plane.HitTest(inFace[i], inFace[next_i], &t, &(vFromPlusToMinus));
				else if (i2 - i > 1)
					vFromPlusToMinus = inFace[(i + 1) % iPtSize];
			}
			else if (signInfoList[i] == -1 && signInfoList[next_i] == 1)
			{
				nIndexFromMinusToPlus = next_i;
				float t(0);
				if (i2 - i == 1)
					plane.HitTest(inFace[i], inFace[next_i], &t, &(vFromMinusToPlus));
				else if (i2 - i > 1)
					vFromMinusToPlus = inFace[(i + 1) % iPtSize];
			}
			i = i2;
		}

		assert(nIndexFromPlusToMinus != -1 && nIndexFromMinusToPlus != -1);

		for (int i = 0, iSize(signInfoList.size()); i < iSize; ++i)
		{
			int nIndex = (i + nIndexFromMinusToPlus) % iSize;
			if (nIndex == nIndexFromPlusToMinus)
				break;
			if (signInfoList[nIndex] == 0)
				continue;
			outPosFace.push_back(inFace[nIndex]);
		}

		outPosFace.push_back(vFromPlusToMinus);
		outPosFace.push_back(vFromMinusToPlus);

		for (int i = 0, iSize(signInfoList.size()); i < iSize; ++i)
		{
			int nIndex = (i + nIndexFromPlusToMinus) % iSize;
			if (nIndex == nIndexFromMinusToPlus)
				break;
			if (signInfoList[nIndex] == 0)
				continue;
			outNegFace.push_back(inFace[nIndex]);
		}

		outNegFace.push_back(vFromMinusToPlus);
		outNegFace.push_back(vFromPlusToMinus);

		return AreaUtil::eSR_CROSS;
	}
	else if (bPosPt && !bNegPt)
	{
		return AreaUtil::eSR_POSITIVE;
	}
	else if (!bPosPt && bNegPt)
	{
		return AreaUtil::eSR_NEGATIVE;
	}
	else
	{
		return AreaUtil::eSR_COINCIDENCE;
	}
}

void CBSPTree3D::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "CBSPTree3D");
	pSizer->AddObject(m_BSPTree);
	pSizer->AddObject(this, sizeof(*this));
}

size_t CBSPTree3D::WriteToBuffer(void* pBuffer) const
{
	if (uint32 nNodes = m_BSPTree.size())
	{
		const size_t nBufferSize = sizeof(nNodes) + nNodes * sizeof(BSPTreeNode);

		if (pBuffer)
		{
			uint32 nNodeCount = SwapEndianValue(nNodes);
			memcpy(pBuffer, &nNodeCount, sizeof(nNodeCount));

			BSPTreeNode* pDst = reinterpret_cast<BSPTreeNode*>((uint8*)pBuffer + sizeof(nNodeCount));

			for (uint i = 0; i < nNodes; ++i)
			{
				BSPTreeNode curNode = m_BSPTree[i];
				SwapEndian(curNode.m_PosChild);
				SwapEndian(curNode.m_NegChild);

				memcpy(pDst + i, &curNode, sizeof(BSPTreeNode));
			}
		}

		return nBufferSize;
	}

	return 0;
}

void CBSPTree3D::ReadFromBuffer(const void* pBuffer)
{
	if (pBuffer)
	{
		uint32 nNodes = 0;
		memcpy(&nNodes, pBuffer, sizeof(nNodes));
		SwapEndian(nNodes);

		m_BSPTree.resize(nNodes);

		if (nNodes > 0)
		{
			BSPTreeNode* pSrc = reinterpret_cast<BSPTreeNode*>((uint8*)pBuffer + sizeof(nNodes));

			for (uint i = 0; i < nNodes; ++i)
			{
				memcpy(&m_BSPTree[i], pSrc + i, sizeof(BSPTreeNode));

				SwapEndian(m_BSPTree[i].m_PosChild);
				SwapEndian(m_BSPTree[i].m_NegChild);
			}
		}
	}
}
