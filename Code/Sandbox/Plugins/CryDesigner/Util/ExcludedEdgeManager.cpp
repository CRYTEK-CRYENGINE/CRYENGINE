// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExcludedEdgeManager.h"

namespace Designer
{
void ExcludedEdgeManager::Add(const BrushEdge3D& edge)
{
	m_Edges.push_back(edge);
}

void ExcludedEdgeManager::Clear()
{
	m_Edges.clear();
}

bool ExcludedEdgeManager::GetVisibleEdge(const BrushEdge3D& edge, const BrushPlane& plane, std::vector<BrushEdge3D>& outVisibleEdges) const
{
	BrushLine line(plane.W2P(edge.m_v[0]), plane.W2P(edge.m_v[1]));
	BrushLine invLine(line.GetInverted());

	std::vector<int> indicesOnCoLine;

	for (int i = 0, iSize(m_Edges.size()); i < iSize; ++i)
	{
		BrushLine lineFromRejected(plane.W2P(m_Edges[i].m_v[0]), plane.W2P(m_Edges[i].m_v[1]));
		if (!line.IsEquivalent(lineFromRejected) && !invLine.IsEquivalent(lineFromRejected))
			continue;

		bool bEquivalent = edge.IsEquivalent(m_Edges[i]);
		bool bInverseEquivalent = !bEquivalent ? Comparison::IsEquivalent(edge.m_v[0], m_Edges[i].m_v[1]) && Comparison::IsEquivalent(edge.m_v[1], m_Edges[i].m_v[0]) : true;
		bool bInside = !bInverseEquivalent ? m_Edges[i].ContainVertex(edge.m_v[0]) && m_Edges[i].ContainVertex(edge.m_v[1]) : true;
		if (bEquivalent || bInverseEquivalent || bInside)
			return false;

		indicesOnCoLine.push_back(i);
	}

	for (int i = 0, iSize(indicesOnCoLine.size()); i < iSize; ++i)
	{
		if (edge.GetSubtractedEdges(m_Edges[indicesOnCoLine[i]], outVisibleEdges))
			break;
	}

	return true;
}
}

