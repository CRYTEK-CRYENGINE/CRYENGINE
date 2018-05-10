// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace MNM
{

template<typename TQuery>
void CNavMesh::QueryTrianglesWithProcessing(const aabb_t& queryAabbWorld, const INavMeshQueryFilter* pFilter, TQuery&& query) const
{
	if (pFilter)
	{
		return QueryTrianglesWithProcessingInternal(queryAabbWorld, *pFilter, query);
	}
	else
	{
		const SAcceptAllQueryTrianglesFilter filter;
		return QueryTrianglesWithProcessingInternal(queryAabbWorld, filter, query);
	}
}

template<typename TFilter, typename TQuery>
void CNavMesh::QueryTrianglesWithProcessingInternal(const aabb_t& queryAabbWorld, const TFilter& filter, TQuery&& query) const
{
	const size_t minX = (std::max(queryAabbWorld.min.x, real_t(0)) / real_t(m_params.tileSize.x)).as_uint();
	const size_t minY = (std::max(queryAabbWorld.min.y, real_t(0)) / real_t(m_params.tileSize.y)).as_uint();
	const size_t minZ = (std::max(queryAabbWorld.min.z, real_t(0)) / real_t(m_params.tileSize.z)).as_uint();

	const size_t maxX = (std::max(queryAabbWorld.max.x, real_t(0)) / real_t(m_params.tileSize.x)).as_uint();
	const size_t maxY = (std::max(queryAabbWorld.max.y, real_t(0)) / real_t(m_params.tileSize.y)).as_uint();
	const size_t maxZ = (std::max(queryAabbWorld.max.z, real_t(0)) / real_t(m_params.tileSize.z)).as_uint();

	for (uint y = minY; y <= maxY; ++y)
	{
		for (uint x = minX; x <= maxX; ++x)
		{
			for (uint z = minZ; z <= maxZ; ++z)
			{
				if (const TileID tileID = GetTileID(x, y, z))
				{
					const vector3_t tileOrigin = GetTileOrigin(x, y, z);
					const STile& tile = GetTile(tileID);

					const vector3_t tileMin(0, 0, 0);
					const vector3_t tileMax(m_params.tileSize.x, m_params.tileSize.y, m_params.tileSize.z);

					aabb_t queryAabbTile(queryAabbWorld);
					queryAabbTile.min = vector3_t::maximize(queryAabbTile.min - tileOrigin, tileMin);
					queryAabbTile.max = vector3_t::minimize(queryAabbTile.max - tileOrigin, tileMax);

					if (QueryTileTrianglesWithProcessing(tileID, tile, queryAabbTile, filter, query) == INavMeshQueryProcessing::EResult::Stop)
						return;
				}
			}
		}
	}
}

template<typename TFilter, typename TQuery>
INavMeshQueryProcessing::EResult CNavMesh::QueryTileTrianglesWithProcessing(const TileID tileID, const STile& tile, const aabb_t& queryAabbTile, TFilter& filter, TQuery&& query) const
{
	const size_t trianglesBatchCount = 32;
	const Tile::STriangle* trianglesToProcess[trianglesBatchCount];
	TriangleID triangleIdsToProcess[trianglesBatchCount];
	size_t trianglesCount = 0;

	INavMeshQueryProcessing::EResult result = INavMeshQueryProcessing::EResult::Continue;

	if (tile.nodeCount > 0)
	{
		const size_t nodeCount = tile.nodeCount;
		size_t nodeID = 0;
		while (nodeID < nodeCount)
		{
			const Tile::SBVNode& node = tile.nodes[nodeID];

			if (!queryAabbTile.overlaps(node.aabb))
			{
				nodeID += node.leaf ? 1 : node.offset;
			}
			else
			{
				++nodeID;

				if (!node.leaf)
					continue;

				const uint16 triangleIdx = node.offset;
				const Tile::STriangle& triangle = GetTriangleUnsafe(tileID, triangleIdx);

				if (filter.PassFilter(triangle))
				{
					triangleIdsToProcess[trianglesCount] = ComputeTriangleID(tileID, triangleIdx);
					trianglesToProcess[trianglesCount] = &triangle;

					if (++trianglesCount < trianglesBatchCount)
						continue;

					// Process triangles
					result = query(tileID, trianglesToProcess, triangleIdsToProcess, trianglesCount);
					if (result == INavMeshQueryProcessing::EResult::Stop)
						return result;

					trianglesCount = 0;
				}
			}
		}
	}
	else
	{
		for (size_t i = 0; i < tile.triangleCount; ++i)
		{
			const Tile::STriangle& triangle = tile.triangles[i];

			const Tile::Vertex& v0 = tile.vertices[triangle.vertex[0]];
			const Tile::Vertex& v1 = tile.vertices[triangle.vertex[1]];
			const Tile::Vertex& v2 = tile.vertices[triangle.vertex[2]];

			const aabb_t triaabb(vector3_t::minimize(v0, v1, v2), vector3_t::maximize(v0, v1, v2));

			if (queryAabbTile.overlaps(triaabb))
			{
				if (filter.PassFilter(triangle))
				{
					triangleIdsToProcess[trianglesCount] = ComputeTriangleID(tileID, static_cast<uint16>(i));
					trianglesToProcess[trianglesCount] = &triangle;

					if (++trianglesCount < trianglesBatchCount)
						continue;

					// Process triangles
					result = query(tileID, trianglesToProcess, triangleIdsToProcess, trianglesCount);
					if (result == INavMeshQueryProcessing::EResult::Stop)
						return result;

					trianglesCount = 0;
				}
			}
		}
	}

	if (trianglesCount > 0)
	{
		// Processing rest of the triangles
		result = query(tileID, trianglesToProcess, triangleIdsToProcess, trianglesCount);
	}
	return result;
}
} // namespace MNM