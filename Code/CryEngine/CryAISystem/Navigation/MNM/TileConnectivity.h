// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/NavigationSystem/MNMTile.h>

namespace MNM
{

// Structure of precomputed data used for connecting a tile with adjacent tiles and updating islands
class CTileConnectivityData
{
public:
	struct Edge
	{
		uint16 vertexIndex[2];
		uint16 triangleIndex[2];
		uint16 sidesToCheckMask;

		inline bool IsInternalEdgeOfTriangle(const size_t triangleIdx) const
		{
			return triangleIndex[0] != triangleIndex[1] && (triangleIdx == triangleIndex[0] || triangleIdx == triangleIndex[1]);
		}
		inline bool IsBoundaryEdgeOfTriangle(const size_t triangleIdx) const
		{
			return triangleIndex[0] == triangleIndex[1] && triangleIndex[0] == triangleIdx;
		}
	};

	inline const Edge& GetEdge(const size_t triangleIdx, const uint16 edgeIdx) const
	{
		const size_t adjacencyIndex = GetAdjancencyIdx(triangleIdx, edgeIdx);
		CRY_ASSERT(adjacencyIndex < m_adjacency.size());
		const size_t edgeIndex = m_adjacency[adjacencyIndex];
		CRY_ASSERT(edgeIndex < m_edges.size());
		return m_edges[edgeIndex];
	}

	void ComputeTriangleAdjacency(const Tile::STriangle* pTriangles, const size_t triangleCount, const Tile::Vertex* pVertices, const size_t vertexCount, const vector3_t& tileSize);
	
	static size_t ComputeTriangleAdjacency(const Tile::STriangle* pTriangles, const size_t triangleCount, const Tile::Vertex* pVertices, const size_t vertexCount, const vector3_t& tileSize, CTileConnectivityData::Edge* pOutEdges, uint16* pOutAdjacency);
	inline static size_t GetAdjancencyIdx(const size_t triangleIdx, const uint16 edgeIdx) 
	{
		return triangleIdx * 3 + edgeIdx;
	}

private:
	static void ComputeSidesToCheckMasks(Edge* pEdges, const size_t edgesCount, const Tile::Vertex* pVertices, const vector3_t& tileSize);

	std::vector<Edge> m_edges;
	std::vector<uint16> m_adjacency; // array of indices to m_edges, use GetAdjancencyIdx function to get correct index from triangle and edge indices.
};

} // namespace MNM