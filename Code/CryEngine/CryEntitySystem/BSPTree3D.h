// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "AreaUtil.h"

class CBSPTree3D : public IBSPTree3D
{
public:
	CBSPTree3D(const IBSPTree3D::FaceList& faceList);

	virtual bool   IsInside(const Vec3& vPos) const;
	virtual void   GetMemoryUsage(ICrySizer* pSizer) const;

	virtual size_t WriteToBuffer(void* pBuffer) const;
	virtual void   ReadFromBuffer(const void* pBuffer);

private:

	struct BSPTreeNode;
	typedef DynArray<BSPTreeNode> NodeStorage;
	typedef uint                  NodeIndex;

	static const NodeIndex kInvalidNodeIndex = (NodeIndex) - 1;

	void                          BuildTree(const IBSPTree3D::FaceList& faceList, NodeStorage& treeNodes) const;
	static AreaUtil::ESplitResult SplitFaceByPlane(const AreaUtil::CPlaneBase& plane, const IBSPTree3D::CFace& inFace, IBSPTree3D::CFace& outPosFace, IBSPTree3D::CFace& outNegFace);

private:

	struct BSPTreeNode
	{
		AreaUtil::EPointPosEnum IsPointIn(const Vec3& vPos, const NodeStorage& pNodeStorage) const;
		void                    GetMemoryUsage(class ICrySizer* pSizer) const;

		AreaUtil::CPlaneBase    m_Plane;
		NodeIndex               m_PosChild;
		NodeIndex               m_NegChild;
	};

	struct GenerateNodeTask
	{
		GenerateNodeTask() : m_TargetNode(kInvalidNodeIndex) {}

		NodeIndex m_TargetNode;
		FaceList  m_FaceList;
	};

	NodeStorage m_BSPTree;
};