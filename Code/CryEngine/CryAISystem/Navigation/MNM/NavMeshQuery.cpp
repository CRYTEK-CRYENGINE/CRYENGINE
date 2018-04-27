// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavMeshQuery.h"

#include "NavMesh.h"

namespace MNM
{

INavMeshQueryProcessing::EResult CTriangleAtQuery::operator() (const MNM::TileID tileId, const MNM::Tile::STriangle** pTriangles, const MNM::TriangleID* pTriangleIds, const size_t trianglesCount)
{
	MNM::vector3_t a, b, c;

	for (size_t i = 0; i < trianglesCount; ++i)
	{
		pNavMesh->GetVertices(pTriangleIds[i], a, b, c);

		if (PointInTriangle(vector2_t(location), vector2_t(a), vector2_t(b), vector2_t(c)))
		{
			const MNM::vector3_t ptClosest = ClosestPtPointTriangle(location, a, b, c);
			const MNM::real_t::unsigned_overflow_type dSq = (ptClosest - location).lenSqNoOverflow();

			if (dSq < distMinSq)
			{
				distMinSq = dSq;
				closestID = pTriangleIds[i];
			}
		}
	}
	return EResult::Continue;
}

INavMeshQueryProcessing::EResult CNearestTriangleQuery::operator() (const MNM::TileID tileId, const MNM::Tile::STriangle** pTriangles, const MNM::TriangleID* pTriangleIds, const size_t trianglesCount)
{
	MNM::vector3_t a, b, c;

	for (size_t i = 0; i < trianglesCount; ++i)
	{
		const MNM::TriangleID triangleId = pTriangleIds[i];

		if (pNavMesh->GetVertices(triangleId, a, b, c))
		{
			const MNM::vector3_t ptClosest = ClosestPtPointTriangle(localFromPosition, a, b, c);
			const MNM::real_t::unsigned_overflow_type dSq = (ptClosest - localFromPosition).lenSqNoOverflow();

			if (dSq < distMinSq)
			{
				closestID = triangleId;
				distMinSq = dSq;
				closestPos = ptClosest;
			}
		}
	}
	return EResult::Continue;
}

INavMeshQueryProcessing::EResult CGetMeshBordersQueryWithFilter::operator() (const MNM::TileID tileId, const MNM::Tile::STriangle** pTriangles, const MNM::TriangleID* pTriangleIds, const size_t trianglesCount)
{
	const MNM::STile& tile = pNavMesh->GetTile(tileId);
	for (size_t i = 0; i < trianglesCount; ++i)
	{
		const MNM::TriangleID triangleId = pTriangleIds[i];
		const Tile::STriangle& triangle = *pTriangles[i];

		size_t linkedEdges = 0;
		for (size_t l = 0; l < triangle.linkCount; ++l)
		{
			const Tile::SLink& link = tile.GetLinks()[triangle.firstLink + l];
			const size_t edge = link.edge;

			if (link.side == Tile::SLink::Internal)
			{
				const MNM::AreaAnnotation* annotation = pNavMesh->GetTriangleAnnotation(ComputeTriangleID(tileId, link.triangle));
				if (filter.PassFilter(*annotation))
				{
					linkedEdges |= static_cast<size_t>(1) << edge;
				}
			}
			else if (link.side != Tile::SLink::OffMesh)
			{
				TileID neighbourTileID = pNavMesh->GetNeighbourTileID(tileId, link.side);
				const MNM::AreaAnnotation* annotation = pNavMesh->GetTriangleAnnotation(ComputeTriangleID(neighbourTileID, link.triangle));
				if (filter.PassFilter(*annotation))
				{
					linkedEdges |= static_cast<size_t>(1) << edge;
				}
			}
		}

		if (linkedEdges == 7)
			continue;

		vector3_t verts[3];
		pNavMesh->GetVertices(triangleId, verts);

		for (size_t e = 0; e < 3; ++e)
		{
			if ((linkedEdges & (size_t(1) << e)) == 0)
			{
				if (pBordersWithNormalsArray != nullptr)
				{
					const Vec3 v0 = verts[e].GetVec3();
					const Vec3 v1 = verts[(e + 1) % 3].GetVec3();
					const Vec3 vOther = verts[(e + 2) % 3].GetVec3();

					const Vec3 edge = Vec3(v0 - v1).GetNormalized();
					const Vec3 otherEdge = Vec3(v0 - vOther).GetNormalized();
					const Vec3 up = edge.Cross(otherEdge);
					const Vec3 normalOut = up.Cross(edge);

					pBordersWithNormalsArray[bordersCount * 3 + 0] = v0;
					pBordersWithNormalsArray[bordersCount * 3 + 1] = v1;
					pBordersWithNormalsArray[bordersCount * 3 + 2] = normalOut;

					if (++bordersCount == maxBordersCount)
						return EResult::Stop;
				}
				else
				{
					++bordersCount;
				}
			}
		}
	}
	return EResult::Continue;
}

INavMeshQueryProcessing::EResult CGetMeshBordersQueryNoFilter::operator() (const MNM::TileID tileId, const MNM::Tile::STriangle** pTriangles, const MNM::TriangleID* pTriangleIds, const size_t trianglesCount)
{
	const MNM::STile& tile = pNavMesh->GetTile(tileId);
	for (size_t i = 0; i < trianglesCount; ++i)
	{
		const MNM::TriangleID triangleId = pTriangleIds[i];
		const Tile::STriangle& triangle = *pTriangles[i];

		size_t linkedEdges = 0;
		for (size_t l = 0; l < triangle.linkCount; ++l)
		{
			const Tile::SLink& link = tile.GetLinks()[triangle.firstLink + l];
			const size_t edge = link.edge;
			linkedEdges |= static_cast<size_t>(1) << edge;
		}

		if (linkedEdges == 7)
			continue;

		vector3_t verts[3];
		pNavMesh->GetVertices(triangleId, verts);

		for (size_t e = 0; e < 3; ++e)
		{
			if ((linkedEdges & (size_t(1) << e)) == 0)
			{
				if (pBordersWithNormalsArray != nullptr)
				{
					const Vec3 v0 = verts[e].GetVec3();
					const Vec3 v1 = verts[(e + 1) % 3].GetVec3();
					const Vec3 vOther = verts[(e + 2) % 3].GetVec3();

					const Vec3 edge = Vec3(v0 - v1).GetNormalized();
					const Vec3 otherEdge = Vec3(v0 - vOther).GetNormalized();
					const Vec3 up = edge.Cross(otherEdge);
					const Vec3 out = up.Cross(edge);

					pBordersWithNormalsArray[bordersCount * 3 + 0] = v0;
					pBordersWithNormalsArray[bordersCount * 3 + 1] = v1;
					pBordersWithNormalsArray[bordersCount * 3 + 2] = out;

					if (++bordersCount == maxBordersCount)
						return EResult::Stop;
				}
				else
				{
					++bordersCount;
				}
			}
		}
	}
	return EResult::Continue;
}
} // namespace MNM