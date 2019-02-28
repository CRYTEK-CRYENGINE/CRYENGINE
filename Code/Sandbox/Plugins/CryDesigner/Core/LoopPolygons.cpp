// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LoopPolygons.h"
#include "Core/Helper.h"

namespace Designer
{
LoopPolygons::LoopPolygons(PolygonPtr polygon, bool bOptimizePolygon) : m_bOptimizedPolygon(bOptimizePolygon)
{
	std::vector<EdgeList> loops;
	FindLoops(polygon, loops);

	for (int i = 0, nLoopCount(loops.size()); i < nLoopCount; ++i)
	{
		PolygonPtr loopPolygon;
		if (CreateNewPolygonFromEdges(polygon, loops[i], bOptimizePolygon, loopPolygon))
		{
			loopPolygon->SetFlag(polygon->GetFlag());
			if (loopPolygon->IsCCW())
				m_OuterLoops.push_back(loopPolygon);
			else
				m_Holes.push_back(loopPolygon);
		}
	}
}

const std::vector<PolygonPtr>& LoopPolygons::GetIslands() const
{
	if (!m_Islands.empty())
		return m_Islands;

	for (int k = 0; k < m_OuterLoops.size(); k++)
	{
		m_Islands.push_back(m_OuterLoops[k]->Clone());
		for (int i = 0; i < m_Holes.size(); ++i)
		{
			if (m_Islands[k]->IncludeAllEdges(m_Holes[i]))
				m_Islands[k]->Attach(m_Holes[i]);
		}
	}

	return m_Islands;
}

std::vector<PolygonPtr> LoopPolygons::GetAllLoops() const
{
	std::vector<PolygonPtr> loops;
	if (m_OuterLoops.empty())
		loops.insert(loops.end(), m_OuterLoops.begin(), m_OuterLoops.end());
	if (!m_Holes.empty())
		loops.insert(loops.end(), m_Holes.begin(), m_Holes.end());
	return loops;
}

std::vector<PolygonPtr> LoopPolygons::GetOuterClones() const
{
	std::vector<PolygonPtr> clones = m_OuterLoops;
	for (auto ii = clones.begin(); ii != clones.end(); ++ii)
		*ii = (*ii)->Clone();
	return clones;
}

std::vector<PolygonPtr> LoopPolygons::GetHoleClones() const
{
	std::vector<PolygonPtr> clones = m_Holes;
	for (auto ii = clones.begin(); ii != clones.end(); ++ii)
		*ii = (*ii)->Clone();
	return clones;
}

void LoopPolygons::FindLoops(PolygonPtr polygon, std::vector<EdgeList>& outLoopList) const
{
	EdgeSet handledEdgeSet;
	EdgeSet edgeSet;
	EdgeMap edgeMapFrom1stTo2nd;

	for (int i = 0, iEdgeCount(polygon->GetEdgeCount()); i < iEdgeCount; ++i)
	{
		const IndexPair& edge = polygon->GetEdgeIndexPair(i);
		edgeSet.insert(edge);
		edgeMapFrom1stTo2nd[edge.m_i[0]].insert(edge.m_i[1]);
	}

	int nCounter(0);
	auto iEdgeSet = edgeSet.begin();
	for (iEdgeSet = edgeSet.begin(); iEdgeSet != edgeSet.end(); ++iEdgeSet)
	{
		if (handledEdgeSet.find(*iEdgeSet) != handledEdgeSet.end())
			continue;

		std::vector<int> subPiece;
		IndexPair edge = *iEdgeSet;
		handledEdgeSet.insert(edge);

		do
		{
			if (edgeMapFrom1stTo2nd.find(edge.m_i[1]) != edgeMapFrom1stTo2nd.end())
			{
				subPiece.push_back(edge.m_i[0]);
			}
			else
			{
				subPiece.clear();
				break;
			}

			const EdgeIndexSet& secondIndexSet = edgeMapFrom1stTo2nd[edge.m_i[1]];

			if (secondIndexSet.size() == 1)
			{
				edge = IndexPair(edge.m_i[1], *secondIndexSet.begin());
			}
			else if (secondIndexSet.size() > 1)
			{
				int edgeidx = polygon->ChooseNextEdge(edge, secondIndexSet);
				edge = polygon->GetEdgeIndexPair(edgeidx);
			}

			if (handledEdgeSet.find(edge) != handledEdgeSet.end())
			{
				subPiece.clear();
				break;
			}
			if (++nCounter > 10000)
			{
				DESIGNER_ASSERT(0 && "Searching connected an edge doesn't seem possible.");
				return;
			}
		}
		while (edge.m_i[1] != (*iEdgeSet).m_i[0]);

		if (!subPiece.empty())
		{
			EdgeList subLoop;
			subPiece.push_back(edge.m_i[0]);
			for (int i = 0, subPieceCount(subPiece.size()); i < subPieceCount; ++i)
			{
				subLoop.push_back(IndexPair(subPiece[i], subPiece[(i + 1) % subPieceCount]));
				handledEdgeSet.insert(IndexPair(subPiece[i], subPiece[(i + 1) % subPieceCount]));
			}
			outLoopList.push_back(subLoop);
		}
	}
}

bool LoopPolygons::CreateNewPolygonFromEdges(PolygonPtr polygon, const std::vector<IndexPair>& inputEdges, bool bOptimizePolygon, PolygonPtr& outPolygon) const
{
	std::vector<Vertex> vertices;
	std::vector<IndexPair> edges;

	for (int i = 0, iSize(inputEdges.size()); i < iSize; ++i)
	{
		const IndexPair& edge(inputEdges[i]);
		int edgeIndex0(Designer::AddVertex(vertices, polygon->GetVertex(edge.m_i[0])));
		int edgeIndex1(Designer::AddVertex(vertices, polygon->GetVertex(edge.m_i[1])));

		if (edgeIndex0 != edgeIndex1)
			edges.push_back(IndexPair(edgeIndex0, edgeIndex1));
	}

	if (vertices.size() >= 3 && edges.size() >= 3)
	{
		outPolygon = new Polygon(vertices, edges, polygon->GetPlane(), polygon->GetSubMatID(), &polygon->GetTexInfo(), bOptimizePolygon);
		return true;
	}

	outPolygon = NULL;
	return false;
}
}

