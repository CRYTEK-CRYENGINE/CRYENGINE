// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer
{
class BSPTree2DNode;
class BSPTree2D : public _i_reference_target_t
{
public:

	BSPTree2D() : m_pRootNode(NULL)
	{
	}
	~BSPTree2D();

	EPointPosEnum     IsVertexIn(const BrushVec3& vertex) const;
	EIntersectionType HasIntersection(const BrushEdge3D& edge) const;
	bool              IsInside(const BrushEdge3D& edge, bool bCheckCoDiff) const;
	bool              IsOnEdge(const BrushEdge3D& edge) const;

	struct OutputEdges
	{
		BrushEdge3D::List posList;
		BrushEdge3D::List negList;
		BrushEdge3D::List coSameList;
		BrushEdge3D::List coDiffList;
	};

	void GetPartitions(const BrushEdge3D& inEdge, OutputEdges& outEdges) const;

	void BuildTree(const BrushPlane& plane, const std::vector<BrushEdge3D>& edgeList);
	void GetEdgeList(BrushEdge3D::List& outEdgeList) const { GetEdgeList(m_pRootNode, outEdgeList); }

	bool HasNegativeNode() const;

private:

	struct Intersection
	{
		BrushVec2 point;
		int       lineIndex[2];
	};
	typedef std::vector<Intersection> IntersectionList;

	static void           GetEdgeList(BSPTree2DNode* pTree, BrushEdge3D::List& outEdgeList);
	static BSPTree2DNode* ConstructTree(const BrushPlane& plane, const std::vector<BrushEdge3D>& edgeList);

private:
	BSPTree2DNode* m_pRootNode;
};

typedef _smart_ptr<BSPTree2D> BSPTree2DPtr;
}

