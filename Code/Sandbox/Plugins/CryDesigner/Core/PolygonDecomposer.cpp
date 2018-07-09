// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PolygonDecomposer.h"
#include "BSPTree2D.h"
#include "Core/SmoothingGroup.h"
#include "Convexes.h"
#include "LoopPolygons.h"

namespace Designer
{
typedef std::pair<BrushFloat, BrushFloat> YXPair;

bool PolygonDecomposer::TriangulatePolygon(PolygonPtr pPolygon, std::vector<PolygonPtr>& outTrianglePolygons)
{
	FlexibleMesh mesh;
	if (!TriangulatePolygon(pPolygon, mesh))
		return false;

	for (int i = 0, iFaceList(mesh.faceList.size()); i < iFaceList; ++i)
	{
		std::vector<BrushVec3> face;
		face.push_back(BrushVec3(mesh.vertexList[mesh.faceList[i].v[0]].pos));
		face.push_back(BrushVec3(mesh.vertexList[mesh.faceList[i].v[1]].pos));
		face.push_back(BrushVec3(mesh.vertexList[mesh.faceList[i].v[2]].pos));
		PolygonPtr pTriangulatedPolygon = new Polygon(face);
		pTriangulatedPolygon->SetSubMatID(pPolygon->GetSubMatID());
		pTriangulatedPolygon->SetTexInfo(pPolygon->GetTexInfo());
		pTriangulatedPolygon->SetFlag(pPolygon->GetFlag());
		pTriangulatedPolygon->RemoveFlags(ePolyFlag_NonplanarQuad);
		pTriangulatedPolygon->SetPlane(pPolygon->GetPlane());
		outTrianglePolygons.push_back(pTriangulatedPolygon);
	}

	return true;
}

bool PolygonDecomposer::TriangulatePolygon(PolygonPtr pPolygon, FlexibleMesh& outMesh)
{
	if (!pPolygon || pPolygon->IsOpen())
		return false;

	m_bGenerateConvexes = false;

	outMesh.faceList.reserve(pPolygon->GetVertexCount());

	m_pPolygon = pPolygon;
	m_Plane = pPolygon->GetPlane();
	m_SubMatID = pPolygon->GetSubMatID();
	m_nBasedVertexIndex = 0;

	m_pOutData = &outMesh;

	if (pPolygon->CheckFlags(ePolyFlag_NonplanarQuad))
	{
		if (!DecomposeNonplanarQuad())
			return false;
	}
	else
	{
		if (!Decompose())
			return false;
	}

	for (int i = 0, faceCount(outMesh.faceList.size()); i < faceCount; ++i)
		outMesh.faceList[i].nSubset = 0;

	return true;
}

bool PolygonDecomposer::DecomposeNonplanarQuad()
{
	if (!m_pPolygon->CheckFlags(ePolyFlag_NonplanarQuad) || !m_pPolygon->IsValid())
		return false;

	int v_order[] = { 0, 1, 2, 2, 3, 0 };
	int n_order[] = { 2, 0, 2, 2, 1, 2 };

	std::vector<Vertex> vList;
	m_pPolygon->GetLinkedVertices(vList);

	BrushVec3 nList[3] = {
		ComputeNormal(vList[v_order[0]].pos, vList[v_order[1]].pos, vList[v_order[2]].pos),
		ComputeNormal(vList[v_order[3]].pos, vList[v_order[4]].pos, vList[v_order[5]].pos),
		(nList[0] + nList[1]).GetNormalized()
	};

	for (int i = 0; i < 2; ++i)
	{
		int offset = i * 3;
		SMeshFace f;
		for (int k = 0; k < 3; ++k)
		{
			m_pOutData->vertexList.push_back(vList[v_order[offset + k]]);
			m_pOutData->normalList.push_back(nList[n_order[offset + k]]);
			f.v[k] = offset + k;
		}
		m_pOutData->faceList.push_back(f);
	}

	return true;
}

void PolygonDecomposer::DecomposeToConvexes(PolygonPtr pPolygon, Convexes& outConvexes)
{
	if (!pPolygon || pPolygon->IsOpen())
		return;

	if (pPolygon->GetVertexCount() <= 4)
	{
		Convex convex;
		pPolygon->GetLinkedVertices(convex);
		outConvexes.AddConvex(convex);
	}
	else
	{
		m_bGenerateConvexes = true;

		m_pPolygon = pPolygon;
		m_Plane = pPolygon->GetPlane();
		m_SubMatID = pPolygon->GetSubMatID();
		m_nBasedVertexIndex = 0;
		m_pBrushConvexes = &outConvexes;

		FlexibleMesh mesh;
		m_pOutData = &mesh;

		Decompose();
	}
}

bool PolygonDecomposer::Decompose()
{
	PolygonPtr pPolygon = m_pPolygon;
	if (!pPolygon)
		return false;

	if (pPolygon->HasBridgeEdges())
	{
		pPolygon = pPolygon->Clone();
		pPolygon->RemoveBridgeEdges();
	}

	bool bOptimizePolygon = !CheckFlag(eDF_SkipOptimizationOfPolygonResults);

	std::vector<PolygonPtr> outerPolygons = pPolygon->GetLoops(bOptimizePolygon)->GetOuterClones();
	std::vector<PolygonPtr> innerPolygons = pPolygon->GetLoops(bOptimizePolygon)->GetHoleClones();

	for (int k = 0, iOuterPolygonsCount(outerPolygons.size()); k < iOuterPolygonsCount; ++k)
	{
		PolygonPtr pOuterPolygon = outerPolygons[k];
		if (!pOuterPolygon)
			return false;

		IndexList initialIndexList;
		m_VertexList.clear();
		m_PointList.clear();

		if (m_bGenerateConvexes)
			RemoveAllConvexData();

		FillVertexListFromPolygon(pOuterPolygon, m_VertexList);

		if (m_bGenerateConvexes)
		{
			for (int i = 0, iVertexCount(m_VertexList.size()); i < iVertexCount; ++i)
				m_InitialEdgesSortedByEdge.insert(GetSortedEdgePair(i, (i + 1) % iVertexCount));
		}

		int iVertexSize(m_VertexList.size());
		for (int i = 0; i < iVertexSize; ++i)
		{
			initialIndexList.push_back(i);
			m_PointList.push_back(Point(Convert3DTo2D(m_VertexList[i].pos), ((i - 1) + iVertexSize) % iVertexSize, (i + 1) % iVertexSize));
		}

		bool bHasInnerHull = false;
		for (int i = 0, innerPolygonSize(innerPolygons.size()); i < innerPolygonSize; ++i)
		{
			if (!pOuterPolygon->IncludeAllEdges(innerPolygons[i]))
				continue;
			bHasInnerHull = true;
			int nBaseIndex = m_PointList.size();
			std::vector<Vertex> innerVertexList;
			FillVertexListFromPolygon(innerPolygons[i], innerVertexList);
			for (int k = 0, nVertexSize(innerVertexList.size()); k < nVertexSize; ++k)
			{
				int nPrevIndex = k == 0 ? nBaseIndex + nVertexSize - 1 : nBaseIndex + k - 1;
				int nNextIndex = k < nVertexSize - 1 ? m_PointList.size() + 1 : nBaseIndex;
				m_PointList.push_back(Point(Convert3DTo2D(innerVertexList[k].pos), nPrevIndex, nNextIndex));
				initialIndexList.push_back(initialIndexList.size());
			}

			m_VertexList.insert(m_VertexList.end(), innerVertexList.begin(), innerVertexList.end());

			if (m_bGenerateConvexes)
			{
				for (int i = 0, iVertexCount(innerVertexList.size()); i < iVertexCount; ++i)
					m_InitialEdgesSortedByEdge.insert(GetSortedEdgePair(i + nBaseIndex, (i + 1) % iVertexCount + nBaseIndex));
			}
		}

		m_pOutData->vertexList.insert(m_pOutData->vertexList.end(), m_VertexList.begin(), m_VertexList.end());

		if (!DecomposeToTriangules(&initialIndexList, bHasInnerHull))
			return false;

		if (m_bGenerateConvexes)
			CreateConvexes();

		m_nBasedVertexIndex += m_VertexList.size();
	}

	int vertexCount(m_pOutData->vertexList.size());

	const BrushVec3& normal = pPolygon->GetPlane().Normal();
	for (int i = 0; i < vertexCount; ++i)
		m_pOutData->normalList.push_back(normal);

	return true;
}

bool PolygonDecomposer::DecomposeToTriangules(IndexList* pIndexList, bool bHasInnerHull)
{
	bool bSuccessfulTriangulate = false;
	if (IsConvex(*pIndexList, false) && !bHasInnerHull)
		bSuccessfulTriangulate = TriangulateConvex(*pIndexList, m_pOutData->faceList);
	else
		bSuccessfulTriangulate = TriangulateConcave(*pIndexList, m_pOutData->faceList);
	if (!bSuccessfulTriangulate)
		return false;
	return true;
}

void PolygonDecomposer::FillVertexListFromPolygon(PolygonPtr pPolygon, std::vector<Vertex>& outVertexList)
{
	if (pPolygon == NULL)
		return;

	for (int i = 0, iEdgeCount(pPolygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		const IndexPair& rEdge = pPolygon->GetEdgeIndexPair(i);
		outVertexList.push_back(pPolygon->GetVertex(rEdge.m_i[0]));
	}
}

bool PolygonDecomposer::TriangulateConvex(const IndexList& indexList, std::vector<SMeshFace>& outFaceList) const
{
	int iIndexCount(indexList.size());
	if (iIndexCount < 3)
		return false;

	int nStartIndex = 0;

	if (iIndexCount >= 4)
	{
		bool bHasInvalidTriangle = true;
		while (bHasInvalidTriangle)
		{
			bHasInvalidTriangle = false;
			for (int i = 1; i < iIndexCount - 1; ++i)
			{
				if (!HasArea(indexList[nStartIndex], indexList[(nStartIndex + i) % iIndexCount], indexList[(nStartIndex + i + 1) % iIndexCount]))
				{
					bHasInvalidTriangle = true;
					break;
				}
			}
			if (bHasInvalidTriangle)
			{
				if ((++nStartIndex) >= iIndexCount)
				{
					nStartIndex = 0;
					break;
				}
			}
		}
	}

	for (int i = 1; i < iIndexCount - 1; ++i)
		AddTriangle(indexList[nStartIndex], indexList[(nStartIndex + i) % iIndexCount], indexList[(nStartIndex + i + 1) % iIndexCount], outFaceList);

	return true;
}

BSPTree2D* PolygonDecomposer::GenerateBSPTree(const IndexList& indexList) const
{
	std::vector<BrushEdge3D> edgeList;
	for (int i = 0, iIndexListCount(indexList.size()); i < iIndexListCount; ++i)
	{
		int nCurr = indexList[i];
		int nNext = indexList[(i + 1) % iIndexListCount];
		edgeList.push_back(BrushEdge3D(m_VertexList[nCurr].pos, m_VertexList[nNext].pos));
	}
	BSPTree2D* pBSPTree = new BSPTree2D;
	pBSPTree->BuildTree(m_pPolygon->GetPlane(), edgeList);
	return pBSPTree;
}

void PolygonDecomposer::BuildEdgeListHavingSameY(const IndexList& indexList, EdgeListMap& outEdgeList) const
{
	for (int i = 0, nIndexBuffSize(indexList.size()); i < nIndexBuffSize; ++i)
	{
		int nNext = (i + 1) % nIndexBuffSize;

		const BrushVec2& v0 = m_PointList[indexList[i]].pos;
		const BrushVec2& v1 = m_PointList[indexList[nNext]].pos;

		if (DecomposerComp::IsEquivalent(v0.y, v1.y))
		{
			BrushFloat fY = v0.y;
			if (outEdgeList.find(fY) != outEdgeList.end())
			{
				IndexList& edgeList = outEdgeList[fY];
				bool bInserted = false;
				for (int k = 0, edgeListCount(edgeList.size()); k < edgeListCount; ++k)
				{
					if (v0.x < m_PointList[indexList[edgeList[k]]].pos.x)
					{
						edgeList.insert(edgeList.begin() + k, i);
						bInserted = true;
						break;
					}
				}
				if (!bInserted)
					edgeList.push_back(i);
			}
			else
			{
				outEdgeList[fY].push_back(i);
			}
		}
	}
}

void PolygonDecomposer::BuildPointSideList(const IndexList& indexList, const EdgeListMap& edgeListHavingSameY, std::vector<EDirection>& outPointSideList, std::map<PointComp, std::pair<int, int>>& outSortedPointsMap) const
{
	outPointSideList.resize(m_VertexList.size());

	int nLeftTopIndex = FindLeftTopVertexIndex(indexList);
	int nRightBottomIndex = FindRightBottomVertexIndex(indexList);

	for (int i = 0, nIndexBuffSize(indexList.size()); i < nIndexBuffSize; ++i)
	{
		outPointSideList[indexList[i]] = QueryPointSide(i, indexList, nLeftTopIndex, nRightBottomIndex);

		BrushFloat fY(m_PointList[indexList[i]].pos.y);
		int xPriority = outPointSideList[indexList[i]] == eDirection_Left ? 0 : 100;
		PointComp pointComp(m_PointList[indexList[i]].pos.y, xPriority);
		EDirection iSide = outPointSideList[indexList[i]];

		auto ifY = edgeListHavingSameY.find(fY);
		if (ifY != edgeListHavingSameY.end())
		{
			const IndexList& indexEdgeList = ifY->second;
			for (int k = 0, iIndexEdgeCount(indexEdgeList.size()); k < iIndexEdgeCount; ++k)
			{
				if (indexEdgeList[k] == i)
				{
					if (iSide == eDirection_Left)
						pointComp.m_XPriority += k * 2;
					else if (iSide == eDirection_Right)
						pointComp.m_XPriority += k * 2 + 1;
					break;
				}
				else if ((indexEdgeList[k] + 1) % nIndexBuffSize == i)
				{
					if (iSide == eDirection_Left)
						pointComp.m_XPriority += k * 2 + 1;
					else if (iSide == eDirection_Right)
						pointComp.m_XPriority += k * 2;
					break;
				}
			}
		}

		outSortedPointsMap[pointComp] = std::pair<int, int>(indexList[i], i);
	}

	DESIGNER_ASSERT(outSortedPointsMap.size() == indexList.size());
}

bool PolygonDecomposer::TriangulateMonotonePolygon(const IndexList& indexList, std::vector<SMeshFace>& outFaceList) const
{
	BSPTree2DPtr pBSPTree = GenerateBSPTree(indexList);

	EdgeListMap edgeListHavingSameY;
	BuildEdgeListHavingSameY(indexList, edgeListHavingSameY);

	std::vector<EDirection> sideList;
	std::map<PointComp, std::pair<int, int>> yxSortedPoints;
	BuildPointSideList(indexList, edgeListHavingSameY, sideList, yxSortedPoints);

	std::stack<int> vertexStack;
	auto ii = yxSortedPoints.begin();

	vertexStack.push(ii->second.first);
	++ii;
	vertexStack.push(ii->second.first);
	auto ii_prev = ii;
	++ii;

	for (; ii != yxSortedPoints.end(); ++ii)
	{
		int top = vertexStack.top();
		int current = ii->second.first;

		DESIGNER_ASSERT(vertexStack.size() >= 2);

		vertexStack.pop();
		int prev = vertexStack.top();
		vertexStack.push(top);

		EDirection topSide = sideList[top];
		EDirection currentSide = sideList[current];

		if (topSide != currentSide)
		{
			int nBaseFaceIndex = outFaceList.size();

			while (!vertexStack.empty())
			{
				top = vertexStack.top();

				if (IsCCW(current, prev, top))
					AddFace(current, prev, top, indexList, outFaceList);
				else
					AddFace(current, top, prev, indexList, outFaceList);

				prev = top;
				vertexStack.pop();
			}
			vertexStack.push(ii_prev->second.first);
		}
		else if (currentSide == eDirection_Right && IsCCW(current, top, prev) || currentSide == eDirection_Left && IsCW(current, top, prev))
		{
			prev = top;
			vertexStack.pop();
			while (!vertexStack.empty())
			{
				top = vertexStack.top();

				if (IsInside(pBSPTree, current, top))
				{
					if (IsCCW(current, prev, top))
						AddFace(current, prev, top, indexList, outFaceList);
					else
						AddFace(current, top, prev, indexList, outFaceList);
				}
				else
				{
					break;
				}

				prev = top;
				vertexStack.pop();
			}
			vertexStack.push(prev);
		}

		vertexStack.push(ii->second.first);
		ii_prev = ii;
	}

	return true;
}

bool PolygonDecomposer::TriangulateConcave(const IndexList& indexList, std::vector<SMeshFace>& outFaceList)
{
	std::vector<IndexList> monotonePieces;
	if (!DecomposePolygonIntoMonotonePieces(indexList, monotonePieces))
		return false;

	for (int i = 0, iMonotonePiecesCount(monotonePieces.size()); i < iMonotonePiecesCount; ++i)
	{
		if (IsConvex(monotonePieces[i], false))
			TriangulateConvex(monotonePieces[i], outFaceList);
		else
			TriangulateMonotonePolygon(monotonePieces[i], outFaceList);
	}

	return true;
}

bool PolygonDecomposer::IsCCW(int i0, int i1, int i2) const
{
	return IsCCW(m_PointList[i0].pos, m_PointList[i1].pos, m_PointList[i2].pos);
}

BrushFloat PolygonDecomposer::IsCCW(const BrushVec2& prev, const BrushVec2& current, const BrushVec2& next) const
{
	const BrushVec3& v0(prev);
	const BrushVec3& v1(current);
	const BrushVec3& v2(next);

	BrushVec3 v10 = (v0 - v1).GetNormalized();
	BrushVec3 v12 = (v2 - v1).GetNormalized();

	BrushVec3 v10_x_v12 = v10.Cross(v12);

	return v10_x_v12.z > -kDecomposerEpsilon;
}

BrushFloat PolygonDecomposer::IsCW(const BrushVec2& prev, const BrushVec2& current, const BrushVec2& next) const
{
	return !IsCCW(prev, current, next);
}

bool PolygonDecomposer::IsCCW(const IndexList& indexList, int nCurr) const
{
	int nIndexListCount = indexList.size();
	return IsCCW(indexList[((nCurr - 1) + nIndexListCount) % nIndexListCount], indexList[nCurr], indexList[(nCurr + 1) % nIndexListCount]);
}

bool PolygonDecomposer::IsConvex(const IndexList& indexList, bool bGetPrevNextFromPointList) const
{
	int iIndexCount(indexList.size());
	if (iIndexCount <= 3)
		return true;

	for (int i = 0; i < iIndexCount; ++i)
	{
		int nPrev = bGetPrevNextFromPointList ? m_PointList[indexList[i]].prev : indexList[((i - 1) + iIndexCount) % iIndexCount];
		int nNext = bGetPrevNextFromPointList ? m_PointList[indexList[i]].next : indexList[(i + 1) % iIndexCount];
		if (!IsCCW(nPrev, indexList[i], nNext))
			return false;
	}

	return true;
}

BrushFloat PolygonDecomposer::Cosine(int i0, int i1, int i2) const
{
	const BrushVec2 p10 = (m_PointList[i0].pos - m_PointList[i1].pos).GetNormalized();
	const BrushVec2 p12 = (m_PointList[i2].pos - m_PointList[i1].pos).GetNormalized();
	return p10.Dot(p12);
}

bool PolygonDecomposer::GetNextIndex(int nCurr, const IndexList& indices, int& nOutNextIndex) const
{
	for (int i = 0, iSize(indices.size()); i < iSize; ++i)
	{
		if (indices[i] == nCurr)
		{
			nOutNextIndex = indices[(i + 1) % iSize];
			return true;
		}
	}
	return false;
}

bool PolygonDecomposer::HasArea(int i0, int i1, int i2) const
{
	if (i0 == i1 || i0 == i2 || i1 == i2)
		return false;

	const BrushVec2& p0 = m_PointList[i0].pos;
	const BrushVec2& p1 = m_PointList[i1].pos;
	const BrushVec2& p2 = m_PointList[i2].pos;

	if (p0.IsEquivalent(p1, kDecomposerEpsilon) || p0.IsEquivalent(p2, kDecomposerEpsilon) || p1.IsEquivalent(p2, kDecomposerEpsilon))
		return false;

	if (IsColinear(p0, p1, p2))
		return false;

	return true;
}

void PolygonDecomposer::AddFace(int i0, int i1, int i2, const IndexList& indices, std::vector<SMeshFace>& outFaceList) const
{
	if (!HasArea(i0, i1, i2))
		return;

	bool bAddedSubTriangles = false;
	int faceIndex[3] = { i0, i1, i2 };
	for (int i = 0; i < 3; ++i)
	{
		int nPrev = faceIndex[((i - 1) + 3) % 3];
		int nCurr = faceIndex[i];
		int nNext = faceIndex[(i + 1) % 3];

		int nIndex = 0;
		int nPrevIndex = nNext;
		if (!GetNextIndex(nPrevIndex, indices, nIndex))
			return;
		int nCounter = 0;
		while (nIndex != nCurr)
		{
			if (IsInsideEdge(nNext, nPrev, nIndex))
			{
				if (!(nCurr == i0 && nPrevIndex == i1 && nIndex == i2 || nCurr == i1 && nPrevIndex == i2 && nIndex == i0 || nCurr == i2 && nPrevIndex == i0 && nIndex == i1))
				{
					bAddedSubTriangles = true;
					AddTriangle(nCurr, nPrevIndex, nIndex, outFaceList);
				}
				nPrevIndex = nIndex;
			}
			if (!GetNextIndex(nIndex, indices, nIndex))
				break;
			if ((nCounter++) > indices.size())
			{
				DESIGNER_ASSERT(0 && "This seems to fall into a infinite loop");
				break;
			}
		}
	}

	if (!bAddedSubTriangles)
		AddTriangle(i0, i1, i2, outFaceList);
}

void PolygonDecomposer::AddTriangle(int i0, int i1, int i2, std::vector<SMeshFace>& outFaceList) const
{
	if (!HasArea(i0, i1, i2))
		return;

	SMeshFace newFace;
	int nBaseVertexIndex = m_nBasedVertexIndex;
	int _i0 = i0 + nBaseVertexIndex;
	int _i1 = i1 + nBaseVertexIndex;
	int _i2 = i2 + nBaseVertexIndex;
	if (HasAlreadyAdded(_i0, _i1, _i2, outFaceList))
		return;

	if (m_bGenerateConvexes)
	{
		IndexList triangle(3);
		triangle[0] = i0;
		triangle[1] = i1;
		triangle[2] = i2;
		int nConvexIndex = m_Convexes.size();
		m_Convexes.push_back(triangle);

		std::pair<int, int> e[3] = { GetSortedEdgePair(i0, i1), GetSortedEdgePair(i1, i2), GetSortedEdgePair(i2, i0) };
		for (int i = 0; i < 3; ++i)
		{
			if (m_InitialEdgesSortedByEdge.find(e[i]) != m_InitialEdgesSortedByEdge.end())
				continue;
			m_ConvexesSortedByEdge[e[i]].push_back(nConvexIndex);
			m_EdgesSortedByConvex[nConvexIndex].insert(e[i]);
		}
	}

	newFace.v[0] = _i0;
	newFace.v[1] = _i1;
	newFace.v[2] = _i2;

	outFaceList.push_back(newFace);
}

bool PolygonDecomposer::IsInside(BSPTree2D* pBSPTree, int i0, int i1) const
{
	if (!pBSPTree)
		return true;
	return pBSPTree->IsInside(BrushEdge3D(m_VertexList[i0].pos, m_VertexList[i1].pos), false);
}

bool PolygonDecomposer::IsOnEdge(BSPTree2D* pBSPTree, int i0, int i1) const
{
	if (!pBSPTree)
		return true;
	return pBSPTree->IsOnEdge(BrushEdge3D(m_VertexList[i0].pos, m_VertexList[i1].pos));
}

bool PolygonDecomposer::IsColinear(const BrushVec2& p0, const BrushVec2& p1, const BrushVec2& p2) const
{
	BrushVec2 p10 = (p0 - p1).GetNormalized();
	BrushVec2 p12 = (p2 - p1).GetNormalized();

	if (p10.IsEquivalent(p12, kDecomposerEpsilon) || p10.IsEquivalent(-p12, kDecomposerEpsilon))
		return true;

	return false;
}

bool PolygonDecomposer::HasAlreadyAdded(int i0, int i1, int i2, const std::vector<SMeshFace>& faceList) const
{
	for (int i = 0, iFaceCount(faceList.size()); i < iFaceCount; ++i)
	{
		if (faceList[i].v[0] == i0 && (faceList[i].v[1] == i1 && faceList[i].v[2] == i2 || faceList[i].v[1] == i2 && faceList[i].v[2] == i1))
			return true;
		if (faceList[i].v[1] == i0 && (faceList[i].v[0] == i1 && faceList[i].v[2] == i2 || faceList[i].v[0] == i2 && faceList[i].v[2] == i1))
			return true;
		if (faceList[i].v[2] == i0 && (faceList[i].v[0] == i1 && faceList[i].v[1] == i2 || faceList[i].v[0] == i2 && faceList[i].v[1] == i1))
			return true;
	}
	return false;
}

bool PolygonDecomposer::IsInsideEdge(int i0, int i1, int i2) const
{
	const BrushVec2& v0 = m_PointList[i0].pos;
	const BrushVec2& v1 = m_PointList[i1].pos;
	const BrushVec2& v2 = m_PointList[i2].pos;

	return BrushEdge(v0, v1).IsInside(v2, kDecomposerEpsilon);
}

bool PolygonDecomposer::IsDifferenceOne(int i0, int i1, const IndexList& indexList) const
{
	int nIndexCount = indexList.size();
	if (i0 == 0 && i1 == nIndexCount - 1 || i0 == nIndexCount - 1 && i1 == 0)
		return true;
	return std::abs(i0 - i1) == 1;
}

int PolygonDecomposer::FindLeftTopVertexIndex(const IndexList& indexList) const
{
	return FindExtreamVertexIndex<std::less<YXPair>>(indexList);
}

int PolygonDecomposer::FindRightBottomVertexIndex(const IndexList& indexList) const
{
	return FindExtreamVertexIndex<std::greater<YXPair>>(indexList);
}

template<class _Pr>
int PolygonDecomposer::FindExtreamVertexIndex(const IndexList& indexList) const
{
	std::map<YXPair, int, _Pr> YXSortedList;

	for (int i = 0, nIndexBuffSize(indexList.size()); i < nIndexBuffSize; ++i)
		YXSortedList[YXPair(m_PointList[indexList[i]].pos.y, m_PointList[indexList[i]].pos.x)] = i;

	DESIGNER_ASSERT(!YXSortedList.empty());

	return YXSortedList.begin()->second;
}

PolygonDecomposer::EDirection PolygonDecomposer::QueryPointSide(int nIndex, const IndexList& indexList, int nLeftTopIndex, int nRightBottomIndex) const
{
	int nBoundary[] = { nLeftTopIndex, nRightBottomIndex };
	PolygonDecomposer::EDirection side[] = { eDirection_Left, eDirection_Right };
	int nIndexBuffSize = indexList.size();

	for (int i = 0, iSize(sizeof(nBoundary) / sizeof(*nBoundary)); i < iSize; ++i)
	{
		int nTraceIndex = nBoundary[i];
		while (nTraceIndex != nBoundary[1 - i])
		{
			if (nTraceIndex == nIndex)
				return side[i];
			++nTraceIndex;
			if (nTraceIndex >= nIndexBuffSize)
				nTraceIndex = 0;
		}
	}

	DESIGNER_ASSERT(0 && "Return Value should be either eDirection_Left or eDirection_Right");

	return eDirection_Invalid;
}

int PolygonDecomposer::FindDirectlyLeftEdge(int nBeginIndex, const IndexList& edgeSearchList, const IndexList& indexList) const
{
	int nIndexCount = indexList.size();
	const BrushVec2& point = m_PointList[nBeginIndex].pos;
	const BrushVec2 leftDir(-1.0f, 0);
	BrushFloat nearestDist = 3e10f;
	int nNearestEdge = -1;

	for (int i = 0, iEdgeCount(edgeSearchList.size()); i < iEdgeCount; ++i)
	{
		int nIndex = indexList[edgeSearchList[i]];
		int nNextIndex = m_PointList[nIndex].next;
		BrushEdge edge(m_PointList[nIndex].pos, m_PointList[nNextIndex].pos);
		BrushLine line(edge.m_v[0], edge.m_v[1]);

		if (line.Distance(point) > 0)
			continue;

		if (point.y < edge.m_v[0].y - kDecomposerEpsilon || point.y > edge.m_v[1].y + kDecomposerEpsilon)
			continue;

		BrushFloat dist;
		if (line.HitTest(point, point + leftDir, &dist))
			dist = std::abs(dist);
		else if (DecomposerComp::IsEquivalent(edge.m_v[0].y, point.y) && DecomposerComp::IsEquivalent(edge.m_v[0].y, edge.m_v[1].y))
			dist = std::min((point - edge.m_v[0]).GetLength(), (point - edge.m_v[1]).GetLength());
		else
			continue;

		if (dist < nearestDist)
		{
			nNearestEdge = edgeSearchList[i];
			nearestDist = dist;
		}
	}

	return nNearestEdge;
}

void PolygonDecomposer::EraseElement(int nIndex, IndexList& indexList) const
{
	for (int i = 0, indexCount(indexList.size()); i < indexCount; ++i)
	{
		if (indexList[i] != nIndex)
			continue;
		indexList.erase(indexList.begin() + i);
		return;
	}
}

void PolygonDecomposer::AddDiagonalEdge(int i0, int i1, EdgeSet& diagonalSet) const
{
	int i0Prev = m_PointList[i0].prev;
	int i0Next = m_PointList[i0].next;
	int i1Prev = m_PointList[i1].prev;
	int i1Next = m_PointList[i1].next;

	if (i0Prev == i1 || i0Next == i1 || i1Prev == i0 || i1Next == i0)
		return;

	if (DecomposerComp::IsEquivalent(m_PointList[i0].pos.y, m_PointList[i1].pos.y))
	{
		if (i1 != i0Prev && DecomposerComp::IsEquivalent(m_PointList[i0].pos.y, m_PointList[i0Prev].pos.y) && IsInsideEdge(i0, i1, i0Prev))
		{
			diagonalSet.insert(IndexPair(i1, i0Prev));
			diagonalSet.insert(IndexPair(i0Prev, i1));
			return;
		}
		else if (i1 != i0Next && DecomposerComp::IsEquivalent(m_PointList[i0].pos.y, m_PointList[i0Next].pos.y) && IsInsideEdge(i0, i1, i0Next))
		{
			diagonalSet.insert(IndexPair(i1, i0Next));
			diagonalSet.insert(IndexPair(i0Next, i1));
			return;
		}
		else if (i0 != i1Prev && DecomposerComp::IsEquivalent(m_PointList[i1].pos.y, m_PointList[i1Prev].pos.y) && IsInsideEdge(i0, i1, i1Prev))
		{
			diagonalSet.insert(IndexPair(i0, i1Prev));
			diagonalSet.insert(IndexPair(i1Prev, i0));
			return;
		}
		else if (i0 != i1Next && DecomposerComp::IsEquivalent(m_PointList[i1].pos.y, m_PointList[i1Next].pos.y) && IsInsideEdge(i0, i1, i1Next))
		{
			diagonalSet.insert(IndexPair(i0, i1Next));
			diagonalSet.insert(IndexPair(i1Next, i0));
			return;
		}
	}

	diagonalSet.insert(IndexPair(i0, i1));
	diagonalSet.insert(IndexPair(i1, i0));
}

bool PolygonDecomposer::DecomposePolygonIntoMonotonePieces(const IndexList& indexList, std::vector<IndexList>& outMonatonePieces)
{
	int nIndexCount(indexList.size());

	std::vector<EPointType> pointTypeList;
	IndexList helperList;
	IndexList edgeSearchList;
	EdgeSet diagonalSet;

	std::map<PointComp, int, std::less<PointComp>> yMap;

	for (int i = 0, nIndexCount(indexList.size()); i < nIndexCount; ++i)
	{
		pointTypeList.push_back(QueryPointType(i, indexList));
		PointComp newPoint(m_PointList[indexList[i]].pos.y, (int)((m_PointList[indexList[i]].pos.x) * 10000.0f));
		if (yMap.find(newPoint) != yMap.end())
			--newPoint.m_XPriority;
		yMap[newPoint] = i;
		helperList.push_back(-1);
	}

	auto yIter = yMap.begin();
	for (; yIter != yMap.end(); ++yIter)
	{
		const PointComp& top = yIter->first;
		int index = yIter->second;
		int prev = m_PointList[index].prev;

		switch (pointTypeList[index])
		{
		case ePointType_Start:
			helperList[index] = index;
			edgeSearchList.push_back(index);
			break;

		case ePointType_End:
			if (helperList[prev] != -1)
			{
				if (pointTypeList[helperList[prev]] == ePointType_Merge)
					AddDiagonalEdge(index, helperList[prev], diagonalSet);
				EraseElement(prev, edgeSearchList);
			}
			break;

		case ePointType_Split:
			{
				int nDirectlyLeftEdgeIndex = FindDirectlyLeftEdge(index, edgeSearchList, indexList);

				DESIGNER_ASSERT(nDirectlyLeftEdgeIndex != -1);
				if (nDirectlyLeftEdgeIndex != -1)
				{
					AddDiagonalEdge(index, helperList[nDirectlyLeftEdgeIndex], diagonalSet);
					helperList[nDirectlyLeftEdgeIndex] = index;
				}
				edgeSearchList.push_back(index);
				helperList[index] = index;
			}
			break;

		case ePointType_Merge:
			{
				if (helperList[prev] != -1)
				{
					if (pointTypeList[helperList[prev]] == ePointType_Merge)
						AddDiagonalEdge(index, helperList[prev], diagonalSet);
				}
				EraseElement(prev, edgeSearchList);
				int nDirectlyLeftEdgeIndex = FindDirectlyLeftEdge(index, edgeSearchList, indexList);
				DESIGNER_ASSERT(nDirectlyLeftEdgeIndex != -1);
				if (nDirectlyLeftEdgeIndex != -1)
				{
					if (pointTypeList[helperList[nDirectlyLeftEdgeIndex]] == ePointType_Merge)
						AddDiagonalEdge(index, helperList[nDirectlyLeftEdgeIndex], diagonalSet);
					helperList[nDirectlyLeftEdgeIndex] = index;
				}
			}
			break;

		case ePointType_Regular:
			{
				EDirection interiorDir = QueryInteriorDirection(index, indexList);
				if (interiorDir == eDirection_Right)
				{
					if (helperList[prev] != -1)
					{
						if (pointTypeList[helperList[prev]] == ePointType_Merge)
							AddDiagonalEdge(index, helperList[prev], diagonalSet);
					}
					EraseElement(prev, edgeSearchList);
					edgeSearchList.push_back(index);
					helperList[index] = index;
				}
				else if (interiorDir == eDirection_Left)
				{
					int nDirectlyLeftEdgeIndex = FindDirectlyLeftEdge(index, edgeSearchList, indexList);
					DESIGNER_ASSERT(nDirectlyLeftEdgeIndex != -1);
					if (nDirectlyLeftEdgeIndex != -1)
					{
						if (pointTypeList[helperList[nDirectlyLeftEdgeIndex]] == ePointType_Merge)
							AddDiagonalEdge(index, helperList[nDirectlyLeftEdgeIndex], diagonalSet);
						helperList[nDirectlyLeftEdgeIndex] = index;
					}
				}
				else
				{
					DESIGNER_ASSERT(0 && "Interior should lie to the right or the left of point.");
				}
			}
			break;
		}
	}

	SearchMonotoneLoops(diagonalSet, indexList, outMonatonePieces);
	return true;
}

void PolygonDecomposer::SearchMonotoneLoops(EdgeSet& diagonalSet, const IndexList& indexList, std::vector<IndexList>& monotonePieces) const
{
	int iIndexListCount(indexList.size());

	EdgeSet handledEdgeSet;
	EdgeSet edgeSet;
	EdgeMap edgeMap;

	for (int i = 0; i < iIndexListCount; ++i)
	{
		edgeSet.insert(IndexPair(indexList[i], m_PointList[indexList[i]].next));
		edgeMap[indexList[i]].insert(m_PointList[indexList[i]].next);
	}

	auto iDiagonalEdgeSet = diagonalSet.begin();
	for (; iDiagonalEdgeSet != diagonalSet.end(); ++iDiagonalEdgeSet)
	{
		edgeSet.insert(*iDiagonalEdgeSet);
		edgeMap[(*iDiagonalEdgeSet).m_i[0]].insert((*iDiagonalEdgeSet).m_i[1]);
	}

	int nCounter(0);
	auto iEdgeSet = edgeSet.begin();
	for (iEdgeSet = edgeSet.begin(); iEdgeSet != edgeSet.end(); ++iEdgeSet)
	{
		if (handledEdgeSet.find(*iEdgeSet) != handledEdgeSet.end())
			continue;

		IndexList monotonePiece;
		IndexPair edge = *iEdgeSet;
		handledEdgeSet.insert(edge);

		do
		{
			monotonePiece.push_back(edge.m_i[0]);

			DESIGNER_ASSERT(edgeMap.find(edge.m_i[1]) != edgeMap.end());
			EdgeIndexSet& secondIndexSet = edgeMap[edge.m_i[1]];
			if (secondIndexSet.size() == 1)
			{
				edge = IndexPair(edge.m_i[1], *secondIndexSet.begin());
				secondIndexSet.clear();
			}
			else if (secondIndexSet.size() > 1)
			{
				BrushFloat ccwCosMax = -1.5f;
				BrushFloat cwCosMin = 1.5f;
				int ccwIndex = -1;
				int cwIndex = -1;
				auto iSecondIndexSet = secondIndexSet.begin();
				for (; iSecondIndexSet != secondIndexSet.end(); ++iSecondIndexSet)
				{
					if (*iSecondIndexSet == edge.m_i[0])
						continue;
					BrushFloat cosine = Cosine(edge.m_i[0], edge.m_i[1], *iSecondIndexSet);
					if (IsCCW(edge.m_i[0], edge.m_i[1], *iSecondIndexSet))
					{
						if (cosine > ccwCosMax)
						{
							ccwIndex = *iSecondIndexSet;
							ccwCosMax = cosine;
						}
					}
					else if (cosine < cwCosMin)
					{
						cwIndex = *iSecondIndexSet;
						cwCosMin = cosine;
					}
				}

				if (ccwIndex != -1)
				{
					edge = IndexPair(edge.m_i[1], ccwIndex);
					secondIndexSet.erase(ccwIndex);
				}
				else
				{
					edge = IndexPair(edge.m_i[1], cwIndex);
					secondIndexSet.erase(cwIndex);
				}
			}
			handledEdgeSet.insert(edge);
			if (++nCounter > 10000)
			{
				DESIGNER_ASSERT(0 && "PolygonDecomposer::SearchMonotoneLoops() - Searching connected an edge doesn't seem possible.");
				return;
			}
		}
		while (edge.m_i[1] != (*iEdgeSet).m_i[0]);

		monotonePiece.push_back(edge.m_i[0]);
		monotonePieces.push_back(monotonePiece);
	}
}

PolygonDecomposer::EPointType PolygonDecomposer::QueryPointType(int nIndex, const IndexList& indexList) const
{
	int iIndexCount(indexList.size());

	int nCurrIndex = indexList[nIndex];
	int nPrevIndex = m_PointList[nCurrIndex].prev;
	int nNextIndex = m_PointList[nCurrIndex].next;

	const BrushVec2& prev = m_PointList[nPrevIndex].pos;
	const BrushVec2& current = m_PointList[nIndex].pos;
	const BrushVec2& next = m_PointList[nNextIndex].pos;

	bool bCCW = IsCCW(prev, current, next);
	bool bCW = !bCCW;

	if (DecomposerComp::IsEquivalent(prev.y, current.y))
	{
		if (bCW)
		{
			if (QueryInteriorDirection(nIndex, indexList) == eDirection_Left)
				return ePointType_Merge;
			else
				return ePointType_Regular;
		}
		else if (DecomposerComp::IsGreaterEqual(next.y, current.y))
			return ePointType_Start;
		else
			return ePointType_End;
	}
	else if (DecomposerComp::IsEquivalent(current.y, next.y))
	{
		if (bCW)
		{
			if (QueryInteriorDirection(nIndex, indexList) == eDirection_Right)
				return ePointType_Regular;
			else
				return ePointType_Split;
		}
		else if (DecomposerComp::IsGreaterEqual(prev.y, current.y))
			return ePointType_Start;
		else
			return ePointType_End;
	}
	else if (DecomposerComp::IsGreaterEqual(prev.y, current.y) && DecomposerComp::IsGreaterEqual(next.y, current.y))
	{
		if (bCCW)
			return ePointType_Start;
		else
			return ePointType_Split;
	}
	else if (DecomposerComp::IsLessEqual(prev.y, current.y) && DecomposerComp::IsLessEqual(next.y, current.y))
	{
		if (bCCW)
			return ePointType_End;
		else
			return ePointType_Merge;
	}
	else if (DecomposerComp::IsLessEqual(prev.y, current.y) && DecomposerComp::IsGreaterEqual(next.y, current.y) ||
	         DecomposerComp::IsGreaterEqual(prev.y, current.y) && DecomposerComp::IsLessEqual(next.y, current.y))
	{
		return ePointType_Regular;
	}

	DESIGNER_ASSERT(0 && "ePointType_Invalid can't be returned.");
	return ePointType_Invalid;
}

PolygonDecomposer::EDirection PolygonDecomposer::QueryInteriorDirection(int nIndex, const IndexList& indexList) const
{
	int nIndexSize = indexList.size();

	int nCurrIndex = indexList[nIndex];
	int nPrevIndex = m_PointList[nCurrIndex].prev;
	int nNextIndex = m_PointList[nCurrIndex].next;

	const BrushVec2& prev = m_PointList[nPrevIndex].pos;
	const BrushVec2& current = m_PointList[nCurrIndex].pos;
	const BrushVec2& next = m_PointList[nNextIndex].pos;

	if (DecomposerComp::IsGreaterEqual(prev.y, current.y) && DecomposerComp::IsGreaterEqual(current.y, next.y))
		return eDirection_Left;

	if (DecomposerComp::IsLessEqual(prev.y, current.y) && DecomposerComp::IsLessEqual(current.y, next.y))
		return eDirection_Right;

	return eDirection_Invalid;
}

void PolygonDecomposer::RemoveIndexWithSameAdjacentPoint(IndexList& indexList) const
{
	auto ii = indexList.begin();
	int nStart = *ii;

	for (; ii != indexList.end(); )
	{
		int nCurr = *ii;
		int nNext = (ii + 1) == indexList.end() ? nStart : *(ii + 1);

		if (m_VertexList[nCurr].IsEquivalent(m_VertexList[nNext], kDecomposerEpsilon))
			ii = indexList.erase(ii);
		else
			++ii;
	}
}

void PolygonDecomposer::FindMatchedConnectedVertexIndices(int iV0, int iV1, const IndexList& indexList, int& nOutIndex0, int& nOutIndex1) const
{
	int i0 = -1, i1 = -1;
	for (int i = 0, iVertexCount(indexList.size()); i < iVertexCount; ++i)
	{
		int nNext = (i + 1) % iVertexCount;
		if (indexList[i] == iV0 && indexList[nNext] == iV1 || indexList[nNext] == iV0 && indexList[i] == iV1)
		{
			i0 = i;
			i1 = nNext;
			break;
		}
	}
	nOutIndex0 = i0;
	nOutIndex1 = i1;
}

bool PolygonDecomposer::MergeTwoConvexes(int iV0, int iV1, int iConvex0, int iConvex1, IndexList& outMergedPolygon)
{
	DESIGNER_ASSERT(!m_Convexes[iConvex0].empty());
	DESIGNER_ASSERT(!m_Convexes[iConvex1].empty());
	if (m_Convexes[iConvex0].empty() || m_Convexes[iConvex1].empty())
		return false;

	int i00 = -1, i01 = -1;
	FindMatchedConnectedVertexIndices(iV0, iV1, m_Convexes[iConvex0], i00, i01);
	DESIGNER_ASSERT(i00 != -1 && i01 != -1);
	if (i00 == -1 || i01 == -1)
		return false;

	int i10 = -1, i11 = -1;
	FindMatchedConnectedVertexIndices(iV0, iV1, m_Convexes[iConvex1], i10, i11);
	DESIGNER_ASSERT(i10 != -1 && i11 != -1);
	if (i10 == -1 || i11 == -1)
		return false;

	outMergedPolygon.clear();
	for (int i = 0, iVertexCount(m_Convexes[iConvex0].size()); i < iVertexCount; ++i)
		outMergedPolygon.push_back(m_Convexes[iConvex0][(i + i01) % iVertexCount]);
	for (int i = 0, iVertexCount(m_Convexes[iConvex1].size()); i < iVertexCount - 2; ++i)
		outMergedPolygon.push_back(m_Convexes[iConvex1][(i + i11 + 1) % iVertexCount]);

	return true;
}

void PolygonDecomposer::CreateConvexes()
{
	if (!m_bGenerateConvexes)
		return;

	auto ii = m_ConvexesSortedByEdge.begin();
	for (; ii != m_ConvexesSortedByEdge.end(); ++ii)
	{
		DESIGNER_ASSERT(ii->second.size() == 2);
		if (ii->second.size() != 2)
			continue;

		int i0 = ii->first.first;
		int i1 = ii->first.second;

		bool bReflex0 = IsCW(m_PointList[i0].prev, i0, m_PointList[i0].next);
		bool bReflex1 = IsCW(m_PointList[i1].prev, i1, m_PointList[i1].next);

		IndexList mergedPolygon;
		bool bIsResultConvex = false;
		if (!bReflex0 && !bReflex1)
		{
			bIsResultConvex = MergeTwoConvexes(i0, i1, ii->second[0], ii->second[1], mergedPolygon);
		}
		else
		{
			if (MergeTwoConvexes(i0, i1, ii->second[0], ii->second[1], mergedPolygon))
				bIsResultConvex = IsConvex(mergedPolygon, false);
		}

		if (bIsResultConvex)
		{
			int nConvexIndex = ii->second[0];
			m_Convexes[nConvexIndex].clear();
			m_Convexes[ii->second[1]] = mergedPolygon;
			auto iEdge = m_EdgesSortedByConvex[nConvexIndex].begin();
			for (; iEdge != m_EdgesSortedByConvex[nConvexIndex].end(); ++iEdge)
			{
				if (nConvexIndex == m_ConvexesSortedByEdge[*iEdge][0])
				{
					m_ConvexesSortedByEdge[*iEdge][0] = ii->second[1];
					m_EdgesSortedByConvex[ii->second[1]].insert(*iEdge);
				}
				if (nConvexIndex == m_ConvexesSortedByEdge[*iEdge][1])
				{
					m_ConvexesSortedByEdge[*iEdge][1] = ii->second[1];
					m_EdgesSortedByConvex[ii->second[1]].insert(*iEdge);
				}
			}
			m_EdgesSortedByConvex.erase(nConvexIndex);
		}
	}

	for (int i = 0, iConvexCount(m_Convexes.size()); i < iConvexCount; ++i)
	{
		if (m_Convexes[i].empty())
			continue;
		Convex convex;
		for (int k = 0, iVListCount(m_Convexes[i].size()); k < iVListCount; ++k)
			convex.push_back(m_VertexList[m_Convexes[i][k]]);
		m_pBrushConvexes->AddConvex(convex);
	}
}

BrushVec2 PolygonDecomposer::Convert3DTo2D(const BrushVec3& pos) const
{
	static BrushMatrix33 tmRot = Matrix33::CreateRotationZ(PI * 0.09f);
	BrushVec3 rotatedPos = tmRot.TransformVector(m_Plane.W2P(pos));
	return BrushVec2(rotatedPos.x, rotatedPos.y);
}

}

