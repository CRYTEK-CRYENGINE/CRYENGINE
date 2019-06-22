// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "NavMeshQueryProcessing.h"
#include "MNMUtils.h"

namespace MNM
{
	//====================================================================
	// CDefaultProcessing
	//====================================================================

	INavMeshQueryProcessing::EResult CDefaultProcessing::Process()
	{
		// Does nothing
		return INavMeshQueryProcessing::EResult::Continue;
	}


	//====================================================================
	// CNavMeshQueryProcessing
	//====================================================================

	bool CNavMeshQueryProcessing::UpdateNavMeshPointerBeforeProcessing()
	{
		const NavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem;

		IF_UNLIKELY(!pNavigationSystem)
		{
			CRY_ASSERT_MESSAGE(pNavigationSystem, "CNavMeshQueryProcessingBase::UpdateNavMeshPointerBeforeProcessing: Couldn't retrieve Navigation Systen from gAIEnv.");
			return false;
		}

		m_pNavMesh = &gAIEnv.pNavigationSystem->GetMesh(m_meshId).navMesh;
		IF_UNLIKELY(!m_pNavMesh)
		{
			CRY_ASSERT_MESSAGE(m_pNavMesh, "CNavMeshQueryProcessingBase::UpdateNavMeshPointerBeforeProcessing: Couldn't retrieve NavMesh pointer from stored mesh id.");
			return false;
		}
		return true;
	}

	//====================================================================
	// CTriangleAtQuery
	//====================================================================

	INavMeshQueryProcessing::EResult CTriangleAtQueryProcessing::Process()
	{
		if (!UpdateNavMeshPointerBeforeProcessing())
		{
			return INavMeshQueryProcessing::EResult::Stop;
		}

		vector3_t a, b, c;

		for (const TriangleID triangleId : m_triangleIdArray)
		{
			m_pNavMesh->GetVertices(triangleId, a, b, c);

			if (Utils::PointInTriangle(vector2_t(m_location), vector2_t(a), vector2_t(b), vector2_t(c)))
			{
				const vector3_t ptClosest = Utils::ClosestPtPointTriangle(m_location, a, b, c);
				const real_t::unsigned_overflow_type dSq = (ptClosest - m_location).lenSqNoOverflow();

				if (dSq < m_distMinSq)
				{
					m_distMinSq = dSq;
					m_closestID = triangleId;
				}
			}
		}

		m_triangleIdArray.clear();
		return INavMeshQueryProcessing::EResult::Continue;
	}

	//====================================================================
	// CNearestTriangleQuery
	//====================================================================

	INavMeshQueryProcessing::EResult CNearestTriangleQueryProcessing::Process()
	{
		if (!UpdateNavMeshPointerBeforeProcessing())
		{
			return INavMeshQueryProcessing::EResult::Stop;
		}

		vector3_t a, b, c;

		for (const TriangleID triangleId : m_triangleIdArray)
		{
			if (m_pNavMesh->GetVertices(triangleId, a, b, c))
			{
				const vector3_t ptClosest = Utils::ClosestPtPointTriangle(m_localFromPosition, a, b, c);
				const real_t::unsigned_overflow_type dSq = (ptClosest - m_localFromPosition).lenSqNoOverflow();

				if (dSq < m_distMinSq)
				{
					m_closestID = triangleId;
					m_distMinSq = dSq;
					m_closestPos = ptClosest;
				}
			}
		}

		m_triangleIdArray.clear();
		return INavMeshQueryProcessing::EResult::Continue;
	}

	//====================================================================
	// CGetMeshBordersQuery
	//====================================================================

	INavMeshQueryProcessing::EResult CGetMeshBordersQueryProcessing::Process()
	{
		if (!UpdateNavMeshPointerBeforeProcessing())
		{
			return INavMeshQueryProcessing::EResult::Stop;
		}

		for (const TriangleID triangleId : m_triangleIdArray)
		{
			const TileID tileId = ComputeTileID(triangleId);
			const STile& tile = m_pNavMesh->GetTile(tileId);

			Tile::STriangle triangle;
			m_pNavMesh->GetTriangle(triangleId, triangle);

			size_t linkedEdges = 0;
			for (size_t l = 0; l < triangle.linkCount; ++l)
			{
				const Tile::SLink& link = tile.GetLinks()[triangle.firstLink + l];
				const size_t edge = link.edge;

				if (link.side == Tile::SLink::Internal)
				{
					const AreaAnnotation* annotation = m_pNavMesh->GetTriangleAnnotation(ComputeTriangleID(tileId, link.triangle));
					if (m_annotationFilter.PassFilter(*annotation))
					{
						linkedEdges |= static_cast<size_t>(1) << edge;
					}
				}
				else if (link.side != Tile::SLink::OffMesh)
				{
					TileID neighbourTileID = m_pNavMesh->GetNeighbourTileID(tileId, link.side);
					const AreaAnnotation* annotation = m_pNavMesh->GetTriangleAnnotation(ComputeTriangleID(neighbourTileID, link.triangle));
					if (m_annotationFilter.PassFilter(*annotation))
					{
						linkedEdges |= static_cast<size_t>(1) << edge;
					}
				}
			}

			if (linkedEdges == 7)
				continue;

			vector3_t verts[3];
			m_pNavMesh->GetVertices(triangleId, verts);

			for (size_t e = 0; e < 3; ++e)
			{
				if ((linkedEdges & (size_t(1) << e)) == 0)
				{
					const Vec3 v0 = verts[e].GetVec3();
					const Vec3 v1 = verts[(e + 1) % 3].GetVec3();
					const Vec3 vOther = verts[(e + 2) % 3].GetVec3();

					const Vec3 edge = Vec3(v0 - v1).GetNormalized();
					const Vec3 otherEdge = Vec3(v0 - vOther).GetNormalized();
					const Vec3 up = edge.Cross(otherEdge);
					const Vec3 normalOut = up.Cross(edge);

					m_bordersNormals.emplace_back(v0, v1, normalOut);
				}
			}
		}

		m_triangleIdArray.clear();
		return EResult::Continue;
	}

	//====================================================================
	// CFindTriangleQuery
	//====================================================================

	INavMeshQueryProcessing::EResult CFindTriangleQueryProcessing::Process()
	{
		if (!UpdateNavMeshPointerBeforeProcessing())
		{
			return INavMeshQueryProcessing::EResult::Stop;
		}

		vector3_t a, b, c;

		for (const TriangleID triangleId : m_triangleIdArray)
		{
			m_pNavMesh->GetVertices(triangleId, a, b, c);

			if (triangleId == m_triangleId && Utils::PointInTriangle(vector2_t(m_location), vector2_t(a), vector2_t(b), vector2_t(c)))
			{
				m_bFound = true;
				m_triangleIdArray.clear();
				return INavMeshQueryProcessing::EResult::Stop;
			}
		}

		m_triangleIdArray.clear();
		return INavMeshQueryProcessing::EResult::Continue;
	}

	//====================================================================
	// CTriangleCenterInMeshQuery
	//====================================================================

	INavMeshQueryProcessing::EResult CTriangleCenterInMeshQueryProcessing::Process()
	{
		if (!UpdateNavMeshPointerBeforeProcessing())
		{
			return INavMeshQueryProcessing::EResult::Stop;
		}

		vector3_t a, b, c;
		for (const TriangleID triangleId : m_triangleIdArray)
		{
			if (m_pNavMesh->GetVertices(triangleId, a, b, c))
			{
				m_triangleCenterArray.emplace_back(m_pNavMesh->ToWorldSpace((a + b + c) * real_t(0.33333f)).GetVec3());
			}

		}
		m_triangleIdArray.clear();
		return INavMeshQueryProcessing::EResult::Continue;
	}
}