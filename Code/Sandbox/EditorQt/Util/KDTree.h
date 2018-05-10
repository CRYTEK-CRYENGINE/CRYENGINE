// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  (c) 2001 - 2012 Crytek GmbH
// -------------------------------------------------------------------------
//  File name:   KDTree.h
//  Created:     Sep/10/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

class KDTreeNode;

class CKDTree
{
public:

	CKDTree();
	~CKDTree();

	bool Build(IStatObj* pStatObj);
	bool FindNearestVertex(const Vec3& raySrc, const Vec3& rayDir, float vVertexBoxSize, const Vec3& localCameraPos, Vec3& outPos, Vec3& vOutHitPosOnCube) const;
	void GetPenetratedBoxes(const Vec3& raySrc, const Vec3& rayDir, std::vector<AABB>& outBoxes);

	enum ESplitAxis
	{
		eSA_X = 0,
		eSA_Y,
		eSA_Z,
		eSA_Invalid
	};

	struct SStatObj
	{
		Matrix34             tm;
		_smart_ptr<IStatObj> pStatObj;
	};

private:

	void BuildRecursively(KDTreeNode* pNode, const AABB& boundbox, std::vector<uint32>& indices) const;
	bool FindNearestVertexRecursively(KDTreeNode* pNode, const Vec3& raySrc, const Vec3& rayDir, float vVertexBoxSize, const Vec3& localCameraPos, Vec3& outPos, Vec3& vOutHitPosOnCube) const;
	void GetPenetratedBoxesRecursively(KDTreeNode* pNode, const Vec3& raySrc, const Vec3& rayDir, std::vector<AABB>& outBoxes);
	void ConstructStatObjList(IStatObj* pStatObj, const Matrix34& matParent);

	static const int s_MinimumVertexSizeInLeafNode = 4;

private:

	KDTreeNode*           m_pRootNode;
	std::vector<SStatObj> m_StatObjectList;

};

