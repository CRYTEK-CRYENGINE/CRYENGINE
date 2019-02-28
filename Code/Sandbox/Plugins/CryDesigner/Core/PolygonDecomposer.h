// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Polygon.h"

namespace Designer
{
class BSPTree2D;
class Convexes;

static const BrushFloat kDecomposerEpsilon = 1.0e-5f;

namespace DecomposerComp
{
template<class _Type>
bool IsEquivalent(const _Type& v0, const _Type& v1)
{
	return std::abs(v0 - v1) < kDecomposerEpsilon;
}

template<class _Type>
bool IsGreaterEqual(const _Type& v0, const _Type& v1)
{
	return v0 - v1 >= -kDecomposerEpsilon;
}

template<class _Type>
bool IsGreater(const _Type& v0, const _Type& v1)
{
	return v0 - v1 > -kDecomposerEpsilon;
}

template<class _Type>
bool IsLessEqual(const _Type& v0, const _Type& v1)
{
	return v0 - v1 <= kDecomposerEpsilon;
}

template<class _Type>
bool IsLess(const _Type& v0, const _Type& v1)
{
	return v0 - v1 < kDecomposerEpsilon;
}
}

enum EDecomposerFlag
{
	eDF_SkipOptimizationOfPolygonResults = BIT(0)
};

//! This class is in charge of decomposing a polygon into triangles and convex hulls without adding any vertices.
//! In order to achieve this precise and compact triangulation, the sweep line triangulation algorithm with monotone polygons is used
//! and the triangles made by this triangulation algorithm are used to create convex hulls.
//! This convex hull algorithm presented by Hertel and Mehlhorn(1983) is simple and fast but the result is suboptimal.
class PolygonDecomposer
{
public:
	PolygonDecomposer(int nFlag = 0) :
		m_nFlag(nFlag),
		m_bGenerateConvexes(false),
		m_pOutData(NULL)
	{
	}

	void DecomposeToConvexes(PolygonPtr pPolygon, Convexes& outConvexes);
	bool TriangulatePolygon(PolygonPtr pPolygon, FlexibleMesh& outMesh);
	bool TriangulatePolygon(PolygonPtr pPolygon, std::vector<PolygonPtr>& outTrianglePolygons);

private:
	enum EPointType
	{
		ePointType_Invalid,
		ePointType_Start,
		ePointType_End,
		ePointType_Regular,
		ePointType_Split,
		ePointType_Merge
	};

	enum EDirection
	{
		eDirection_Invalid,
		eDirection_Right,
		eDirection_Left
	};

	struct Point
	{
		Point(const BrushVec2& _pos, int _prev, int _next) :
			pos(_pos),
			prev(_prev),
			next(_next)
		{
		}
		BrushVec2 pos;
		int       prev;
		int       next;
	};

	typedef std::vector<int> IndexList;

private:

	struct less_BrushFloat
	{
		bool operator()(const BrushFloat& v0, const BrushFloat& v1) const
		{
			return DecomposerComp::IsLess(v0, v1);
		}
	};

	struct PointComp
	{
		PointComp(const BrushFloat yPos, int xPriority)
		{
			m_yPos = yPos;
			m_XPriority = xPriority;
		}

		BrushFloat m_yPos;
		int        m_XPriority;

		bool operator<(const PointComp& point) const
		{
			if (DecomposerComp::IsLess(m_yPos, point.m_yPos))
				return true;
			if (DecomposerComp::IsEquivalent(m_yPos, point.m_yPos) && m_XPriority < point.m_XPriority)
				return true;
			return false;
		}

		bool operator>(const PointComp& point) const
		{
			if (DecomposerComp::IsGreater(m_yPos, point.m_yPos))
				return true;
			if (DecomposerComp::IsEquivalent(m_yPos, point.m_yPos) && m_XPriority > point.m_XPriority)
				return true;
			return false;
		}
	};

	typedef std::map<BrushFloat, IndexList, less_BrushFloat> EdgeListMap;

private:
	bool        Decompose();
	bool        DecomposeToTriangules(IndexList* pIndexList, bool bHasInnerHull);
	bool        DecomposeNonplanarQuad();
	bool        DecomposePolygonIntoMonotonePieces(const IndexList& indexList, std::vector<IndexList>& outMonatonePieces);

	bool        TriangulateConvex(const IndexList& indexList, std::vector<SMeshFace>& outFaceList) const;
	bool        TriangulateConcave(const IndexList& indexList, std::vector<SMeshFace>& outFaceList);
	bool        TriangulateMonotonePolygon(const IndexList& indexList, std::vector<SMeshFace>& outFaceList) const;

	void        BuildEdgeListHavingSameY(const IndexList& indexList, EdgeListMap& outEdgeList) const;
	void        BuildPointSideList(const IndexList& indexList, const EdgeListMap& edgeListHavingSameY, std::vector<EDirection>& outPointSideList, std::map<PointComp, std::pair<int, int>>& outSortedMarksMap) const;

	void        AddTriangle(int i0, int i1, int i2, std::vector<SMeshFace>& outFaceList) const;
	void        AddFace(int i0, int i1, int i2, const IndexList& indices, std::vector<SMeshFace>& outFaceList) const;
	bool        IsInside(BSPTree2D* pBSPTree, int i0, int i1) const;
	bool        IsOnEdge(BSPTree2D* pBSPTree, int i0, int i1) const;
	bool        IsCCW(int i0, int i1, int i2) const;
	bool        IsCW(int i0, int i1, int i2) const                { return !IsCCW(i0, i1, i2); }
	bool        IsCCW(const IndexList& indexList, int nCurr) const;
	bool        IsCW(const IndexList& indexList, int nCurr) const { return !IsCCW(indexList, nCurr); }
	BrushFloat  IsCCW(const BrushVec2& prev, const BrushVec2& current, const BrushVec2& next) const;
	BrushFloat  IsCW(const BrushVec2& prev, const BrushVec2& current, const BrushVec2& next) const;
	bool        IsConvex(const IndexList& indexList, bool bGetPrevNextFromPointList) const;
	BrushFloat  Cosine(int i0, int i1, int i2) const;
	bool        HasAlreadyAdded(int i0, int i1, int i2, const std::vector<SMeshFace>& faceList) const;
	bool        IsColinear(const BrushVec2& p0, const BrushVec2& p1, const BrushVec2& p2) const;
	bool        IsDifferenceOne(int i0, int i1, const IndexList& indexList) const;
	bool        IsInsideEdge(int i0, int i1, int i2) const;
	bool        GetNextIndex(int nCurr, const IndexList& indices, int& nOutNextIndex) const;
	bool        HasArea(int i0, int i1, int i2) const;

	int         FindLeftTopVertexIndex(const IndexList& indexList) const;
	int         FindRightBottomVertexIndex(const IndexList& indexList) const;
	template<class _Pr>
	int         FindExtreamVertexIndex(const IndexList& indexList) const;

	EDirection  QueryPointSide(int nIndex, const IndexList& indexList, int nLeftTopIndex, int nRightBottomIndex) const;
	EPointType  QueryPointType(int nIndex, const IndexList& indexList) const;
	EDirection  QueryInteriorDirection(int nIndex, const IndexList& indexList) const;

	int         FindDirectlyLeftEdge(int nBeginIndex, const IndexList& edgeSearchList, const IndexList& indexList) const;
	void        EraseElement(int nIndex, IndexList& edgeSearchList) const;

	void        SearchMonotoneLoops(EdgeSet& diagonalSet, const IndexList& indexList, std::vector<IndexList>& monotonePieces) const;
	void        AddDiagonalEdge(int i0, int i1, EdgeSet& diagonalList) const;
	BSPTree2D*  GenerateBSPTree(const IndexList& indexList) const;

	void        RemoveIndexWithSameAdjacentPoint(IndexList& indexList) const;
	static void FillVertexListFromPolygon(PolygonPtr pPolygon, std::vector<Vertex>& outVertexList);

	void        CreateConvexes();
	int         CheckFlag(int nFlag) const { return m_nFlag & nFlag; }

	BrushVec2   Convert3DTo2D(const BrushVec3& pos) const;

private: // Related to Triangulation
	int                 m_nFlag;
	FlexibleMesh*       m_pOutData;
	std::vector<Vertex> m_VertexList;
	std::vector<Point>  m_PointList;
	BrushPlane          m_Plane;
	short               m_SubMatID;
	PolygonPtr          m_pPolygon;
	int                 m_nBasedVertexIndex;
	bool                m_bGenerateConvexes;

private: // Related to Decomposition into convexes
	std::pair<int, int> GetSortedEdgePair(int i0, int i1) const
	{
		if (i1 < i0)
			std::swap(i0, i1);
		return std::pair<int, int>(i0, i1);
	}
	void FindMatchedConnectedVertexIndices(int iV0, int iV1, const IndexList& indexList, int& nOutIndex0, int& nOutIndex1) const;
	bool MergeTwoConvexes(int iV0, int iV1, int iConvex0, int iConvex1, IndexList& outMergedPolygon);
	void RemoveAllConvexData()
	{
		m_InitialEdgesSortedByEdge.clear();
		m_ConvexesSortedByEdge.clear();
		m_EdgesSortedByConvex.clear();
		m_Convexes.clear();
	}
	mutable std::set<std::pair<int, int>>                   m_InitialEdgesSortedByEdge;
	mutable std::map<std::pair<int, int>, std::vector<int>> m_ConvexesSortedByEdge;
	mutable std::map<int, std::set<std::pair<int, int>>>    m_EdgesSortedByConvex;
	mutable std::vector<IndexList>                          m_Convexes;
	mutable Convexes* m_pBrushConvexes;
};
}

