// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TileConnectivity.h"
#include "Navigation/MNM/MNMUtils.h"
#include <CryAISystem/NavigationSystem/MNMNavMesh.h>

namespace MNM
{

void CTileConnectivityData::ComputeTriangleAdjacency(const Tile::STriangle* pTriangles, const size_t triangleCount, const Tile::Vertex* pVertices, const size_t vertexCount, const vector3_t& tileSize)
{
	Edge tmpEdges[MNM::Constants::TileTrianglesMaxCount * 3];

	const size_t adjacencyCount = triangleCount * 3;
	m_adjacency.resize(adjacencyCount);

	const size_t edgesCount = ComputeTriangleAdjacency(pTriangles, triangleCount, pVertices, vertexCount, tileSize, tmpEdges, m_adjacency.data());
	m_edges.assign(tmpEdges, tmpEdges + edgesCount);
}

size_t CTileConnectivityData::ComputeTriangleAdjacency(
	const Tile::STriangle* pTriangles, const size_t triangleCount, const Tile::Vertex* pVertices, const size_t vertexCount, const vector3_t& tileSize,
	CTileConnectivityData::Edge* pOutEdges, uint16* pOutAdjacency)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	enum { Unused = 0xffff, };

	//TODO: is this buffer always big enough?
	const size_t MaxLookUp = 4096;
	uint16 edgeLookUp[MaxLookUp];
	CRY_ASSERT(MaxLookUp > vertexCount + triangleCount * 3);

	std::fill(&edgeLookUp[0], &edgeLookUp[0] + vertexCount, static_cast<uint16>(Unused));

	uint16 edgesCount = 0;

	for (uint16 triangleIdx = 0; triangleIdx < triangleCount; ++triangleIdx)
	{
		const Tile::STriangle& triangle = pTriangles[triangleIdx];

		for (uint16 vertexIdx = 0; vertexIdx < 3; ++vertexIdx)
		{
			const uint16 i1 = triangle.vertex[vertexIdx];
			const uint16 i2 = triangle.vertex[Utils::next_mod3(vertexIdx)];

			if (i1 < i2)
			{
				const uint16 edgeIdx = edgesCount++;
				pOutAdjacency[GetAdjancencyIdx(triangleIdx, vertexIdx)] = edgeIdx;

				Edge& edge = pOutEdges[edgeIdx];
				edge.triangleIndex[0] = triangleIdx;
				edge.triangleIndex[1] = triangleIdx;
				edge.vertexIndex[0] = i1;
				edge.vertexIndex[1] = i2;

				edgeLookUp[vertexCount + edgeIdx] = edgeLookUp[i1];
				edgeLookUp[i1] = edgeIdx;
			}
		}
	}

	for (uint16 triangleIdx = 0; triangleIdx < triangleCount; ++triangleIdx)
	{
		const Tile::STriangle& triangle = pTriangles[triangleIdx];

		for (uint16 vertexIdx = 0; vertexIdx < 3; ++vertexIdx)
		{
			const uint16 i1 = triangle.vertex[vertexIdx];
			const uint16 i2 = triangle.vertex[Utils::next_mod3(vertexIdx)];

			if (i1 > i2)
			{
				uint16 edgeIndex = edgeLookUp[i2];
				while (edgeIndex != Unused)
				{
					Edge& edge = pOutEdges[edgeIndex];

					if ((edge.vertexIndex[1] == i1) && (edge.triangleIndex[0] == edge.triangleIndex[1]))
					{
						edge.triangleIndex[1] = triangleIdx;
						pOutAdjacency[GetAdjancencyIdx(triangleIdx, vertexIdx)] = edgeIndex;
						break;
					}
					edgeIndex = edgeLookUp[vertexCount + edgeIndex];
				}

				if (edgeIndex == Unused)
				{
					const uint16 edgeIdx = edgesCount++;
					pOutAdjacency[GetAdjancencyIdx(triangleIdx, vertexIdx)] = edgeIdx;

					Edge& edge = pOutEdges[edgeIdx];
					edge.vertexIndex[0] = i1;
					edge.vertexIndex[1] = i2;
					edge.triangleIndex[0] = triangleIdx;
					edge.triangleIndex[1] = triangleIdx;
				}
			}
		}
	}

	ComputeSidesToCheckMasks(pOutEdges, edgesCount, pVertices, tileSize);

	return edgesCount;
}

// The function computes mask based on the order of sides defined in NeighbourOffset_MeshGrid (MNMNavMesh.h).
void CTileConnectivityData::ComputeSidesToCheckMasks(Edge* pEdges, const size_t edgesCount, const Tile::Vertex* pVertices, const vector3_t& tileSize)
{
	for (uint16 edgeIdx = 0; edgeIdx < edgesCount; ++edgeIdx)
	{
		Edge& edge = pEdges[edgeIdx];

		uint16 sidesMask = 0;
		if (edge.triangleIndex[0] == edge.triangleIndex[1])
		{
			const vector3_t& vertex0 = pVertices[edge.vertexIndex[0]];
			const vector3_t& vertex1 = pVertices[edge.vertexIndex[1]];

			int axisTest[3];
			for (size_t dim = 0; dim < 3; ++dim)
			{
				axisTest[dim] = vertex0[dim] == vertex1[dim] ? 1 : 0;
			}

			for (size_t dim = 0; dim < 2; ++dim)
			{
				if (axisTest[dim])
				{
					size_t side = dim * 3;
					if (vertex0[dim] == 0)
					{
						side += 7;
					}
					else if (vertex0[dim] != tileSize[dim])
					{
						continue;
					}
					CRY_ASSERT(side < 14);
					sidesMask = BIT16(side);

					if (axisTest[2])
					{
						if (vertex0[2] == 0)
							side += vertex0[dim] == 0 ? 1 : 2;
						if (vertex0[2] == tileSize[2])
							side += vertex0[dim] == 0 ? 2 : 1;
					}
					CRY_ASSERT(side < 14);
					sidesMask |= BIT16(side);
				}
			}
			if (sidesMask == 0 && axisTest[2])
			{
				if (vertex0[2] == 0)
					sidesMask = BIT16(13);
				if (vertex0[2] == tileSize[2])
					sidesMask = BIT16(6);
			}
		}
		edge.sidesToCheckMask = sidesMask;
	}
}

} // namespace MNM