// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "NavMesh.h"
#include "NavMeshQuery.h"
#include "../NavigationSystem/NavigationSystem.h"

#ifdef NAV_MESH_QUERY_DEBUG
#include <CryUDR/InterfaceIncludes.h>
#endif // NAV_MESH_QUERY_DEBUG

namespace MNM
{

namespace DefaultQueryFilters
{
	SQueryTrianglesFilterGlobal g_globalFilter;
	SNavMeshQueryFilterDefault g_globalFilterVirtual;
	SAcceptAllQueryTrianglesFilter g_acceptAllFilterVirtual;
}

//====================================================================
// DebugDraw
//====================================================================


#ifdef NAV_MESH_QUERY_DEBUG
void INavMeshQuery::SNavMeshQueryConfig::DebugDraw() const
{
	const ColorF queryAabbColor = Col_Yellow;

	Cry::UDR::CScope_FixedString configScope("Config");
	{
		stack_string meshIdText;
		meshIdText.Format("Mesh Id: %u", meshId);
		{
			Cry::UDR::CScope_FixedString meshIdScope(meshIdText.c_str());
		}

		stack_string callerNameText;
		callerNameText.Format("Caller: %s", szCallerName);
		{
			Cry::UDR::CScope_FixedString callerNameScope(callerNameText.c_str());
		}

		stack_string aabbText;
		aabbText.Format("AABB: (%d, %d, %d) (%d, %d, %d)", aabb.min.x.as_int(), aabb.min.y.as_int(), aabb.min.z.as_int(), aabb.max.x.as_int(), aabb.max.y.as_int(), aabb.max.z.as_int());
		{
			Cry::UDR::CScope_FixedString aabbScope(aabbText.c_str());
			aabbScope->AddAABB(AABB(aabb.min.GetVec3(), aabb.max.GetVec3()), queryAabbColor);
		}

		stack_string overlappingText;
		overlappingText.Format("Overlapping Mode: %s", Serialization::getEnumLabel(overlappingMode));
		{
			Cry::UDR::CScope_FixedString overlappingModeScope(overlappingText.c_str());
		}

		stack_string queryFilterText;
		queryFilterText.Format("Query Filter: %s", pQueryFilter ? "Yes" : "No");
		{
			Cry::UDR::CScope_FixedString queryFilterScope(queryFilterText.c_str());
		}
	}
}

void INavMeshQuery::SNavMeshQueryConfigBatch::DebugDraw() const
{
	SNavMeshQueryConfig::DebugDraw();

	Cry::UDR::CScope_FixedString configScope("Config");
	{
		stack_string processingTriangleMaxSizeText;
		processingTriangleMaxSizeText.Format("Processing Triangles Max Size: %lu", processingTrianglesMaxSize);
		{
			Cry::UDR::CScope_FixedString processingTriangleMaxSizeScope(processingTriangleMaxSizeText.c_str());
		}

		stack_string actionOnNavMeshChangeText;
		actionOnNavMeshChangeText.Format("Action On NavMesh Change: %s", Serialization::getEnumLabel(actionOnNavMeshRegeneration));
		{
			Cry::UDR::CScope_FixedString actionOnNavMeshChangeScope(actionOnNavMeshChangeText.c_str());
		}

		stack_string actionOnNavMeshAnnotationChangeText;
		actionOnNavMeshAnnotationChangeText.Format("Action On NavMesh Annotation Change: %s", Serialization::getEnumLabel(actionOnNavMeshAnnotationChange));
		{
			Cry::UDR::CScope_FixedString actionOnNavMeshAnnotationChangeScope(actionOnNavMeshAnnotationChangeText.c_str());
		}
	}
}
#endif // NAV_MESH_QUERY_DEBUG



//====================================================================
// CNavMeshQuery::CTriangleIterator
//====================================================================

CNavMeshQuery::CTriangleIterator::CTriangleIterator()
	: m_meshId(0)
	, m_pNavMesh(nullptr)
	, m_minTileX(0)
	, m_minTileY(0)
	, m_minTileZ(0)
	, m_maxTileX(0)
	, m_maxTileY(0)
	, m_maxTileZ(0)
	, m_tileId()
	, m_tileX(0)
	, m_tileY(0)
	, m_tileZ(0)
	, m_triangleIdxTile(0)
	, m_hasNext(true)
{
}

CNavMeshQuery::CTriangleIterator::CTriangleIterator(const NavigationMeshID meshId, const aabb_t& queryAabb)
	: m_meshId(meshId)
	, m_pNavMesh(nullptr)
	, m_minTileX(0)
	, m_minTileY(0)
	, m_minTileZ(0)
	, m_maxTileX(0)
	, m_maxTileY(0)
	, m_maxTileZ(0)
	, m_tileId()
	, m_tileX(0)
	, m_tileY(0)
	, m_tileZ(0)
	, m_triangleIdxTile(0)
	, m_hasNext(true)
{
	if (!UpdateNavMeshPointer())
	{
		return;
	}

	const CNavMesh::SGridParams meshParams = m_pNavMesh->GetGridParams();

	m_minTileX = (std::max(queryAabb.min.x, real_t(0)) / real_t(meshParams.tileSize.x)).as_uint();
	m_minTileY = (std::max(queryAabb.min.y, real_t(0)) / real_t(meshParams.tileSize.y)).as_uint();
	m_minTileZ = (std::max(queryAabb.min.z, real_t(0)) / real_t(meshParams.tileSize.z)).as_uint();

	m_maxTileX = (std::max(queryAabb.max.x, real_t(0)) / real_t(meshParams.tileSize.x)).as_uint();
	m_maxTileY = (std::max(queryAabb.max.y, real_t(0)) / real_t(meshParams.tileSize.y)).as_uint();
	m_maxTileZ = (std::max(queryAabb.max.z, real_t(0)) / real_t(meshParams.tileSize.z)).as_uint();

	m_tileX = m_minTileX;
	m_tileY = m_minTileY;
	m_tileZ = m_minTileZ;

	m_tileId = m_pNavMesh->GetTileID(m_tileX, m_tileY, m_tileZ);
	m_hasNext = m_tileId.IsValid() || m_minTileX <= m_maxTileX || m_minTileY <= m_maxTileY || m_minTileZ <= m_maxTileZ;
}

NavigationMeshID CNavMeshQuery::CTriangleIterator::GetMeshId() const
{
	return m_meshId;
}

const CNavMesh* CNavMeshQuery::CTriangleIterator::GetNavMesh() const
{
	return m_pNavMesh;
}

size_t CNavMeshQuery::CTriangleIterator::GetMinTileX() const
{
	return m_minTileX;
}

size_t CNavMeshQuery::CTriangleIterator::GetMinTileY() const
{
	return m_minTileY;
}
	
size_t CNavMeshQuery::CTriangleIterator::GetMinTileZ() const
{
	return m_minTileZ;
}

size_t CNavMeshQuery::CTriangleIterator::GetMaxTileX() const
{
	return m_maxTileX;
}

size_t CNavMeshQuery::CTriangleIterator::GetMaxTileY() const
{
	return m_maxTileY;
}

size_t CNavMeshQuery::CTriangleIterator::GetMaxTileZ() const
{
	return m_maxTileZ;
}

bool CNavMeshQuery::CTriangleIterator::HasNext() const
{
	return m_hasNext;
}

bool CNavMeshQuery::CTriangleIterator::UpdateNavMeshPointer()
{
	const NavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem;

	IF_UNLIKELY(m_meshId == NavigationMeshID(0))
	{
		CRY_ASSERT_MESSAGE(m_meshId != NavigationMeshID(0), "CNavMeshIterator::UpdateNavMeshPointer: Stored mesh id is invalid (0)");
		return false;
	}

	IF_UNLIKELY(!pNavigationSystem)
	{
		CRY_ASSERT_MESSAGE(pNavigationSystem, "CNavMeshIterator::UpdateNavMeshPointer: Couldn't retrieve Navigation Systen from gAIEnv.");
		return false;
	}

	m_pNavMesh = &gAIEnv.pNavigationSystem->GetMesh(m_meshId).navMesh;
	IF_UNLIKELY(!m_pNavMesh)
	{
		CRY_ASSERT_MESSAGE(m_pNavMesh, "CNavMeshIterator::UpdateNavMeshPointer: Couldn't retrieve NavMesh pointer from mesh id.");
		return false;
	}

	return true;
}

bool CNavMeshQuery::CTriangleIterator::IsEnabledBoundingVolumes() const
{
	IF_UNLIKELY(!m_tileId.IsValid())
	{
		return false;
	}

	const STile& tile = m_pNavMesh->GetTile(m_tileId);
	return tile.GetBVNodesCount() > 0;
}

bool CNavMeshQuery::CTriangleIterator::Next()
{
	// Doesn't have next
	if (!m_hasNext)
	{
		return false;
	}

	// Current tile is invalid and is the last tile 
	if (!m_tileId.IsValid() && m_tileX == m_maxTileX && m_tileY == m_maxTileY && m_tileZ == m_maxTileZ)
	{
		m_hasNext = false;
		return false;
	}

	// Try moving to next triangle within the current tile
	bool movedToNextTriangle = false;
	if (!IsEnabledBoundingVolumes())
	{
		movedToNextTriangle = NextTriangleLinear();
	}
	else
	{
		movedToNextTriangle = NextTriangleBoundingVolumes();
	}

	if (movedToNextTriangle)
	{
		return true;
	}

	if (NextTile())
	{
		return true;
	}
		
	m_hasNext = false;
	return false;
}

bool CNavMeshQuery::CTriangleIterator::NextTile()
{
	// Reset triangle index and move to the next tile (if there's one)
	if (m_tileX < m_maxTileX || m_tileY < m_maxTileY || m_tileZ < m_maxTileZ)
	{
		m_triangleIdxTile = 0;

		if (m_tileZ < m_maxTileZ)
		{
			++m_tileZ;
		}
		else
		{
			m_tileZ = m_minTileZ;

			if (m_tileX < m_maxTileX)
			{
				++m_tileX;
			}
			else
			{
				m_tileX = m_minTileX;
				++m_tileY;
			}
		}

		m_tileId = m_pNavMesh->GetTileID(m_tileX, m_tileY, m_tileZ);
		return true;
	}

	// No tiles left
	return false;
}

CNavMeshQuery::CTriangleIterator::STriangleInfo CNavMeshQuery::CTriangleIterator::GetTriangle() const
{
	if (!IsEnabledBoundingVolumes())
	{
		return GetTriangleLinear();
	}
	else
	{
		return GetTriangleBoundingVolumes();
	}
}

bool CNavMeshQuery::CTriangleIterator::NextTriangleLinear()
{
	if (m_tileId.IsValid() && m_triangleIdxTile + 1 < m_pNavMesh->GetTile(m_tileId).GetTrianglesCount())
	{
		++m_triangleIdxTile;
		return true;
	}
	return false;
}

CNavMeshQuery::CTriangleIterator::STriangleInfo CNavMeshQuery::CTriangleIterator::GetTriangleLinear() const
{
	IF_UNLIKELY(!m_tileId.IsValid())
	{
		return CTriangleIterator::STriangleInfo();
	}

	const STile& tile = m_pNavMesh->GetTile(m_tileId);
	IF_UNLIKELY(m_triangleIdxTile >= tile.GetTrianglesCount())
	{
		return CTriangleIterator::STriangleInfo();
	}

	const Tile::STriangle& triangle = tile.GetTriangles()[m_triangleIdxTile];
	const TriangleID triangleId = ComputeTriangleID(m_tileId, m_triangleIdxTile);

	return STriangleInfo(m_tileId, triangleId, m_triangleIdxTile, triangle);
}

bool CNavMeshQuery::CTriangleIterator::NextTriangleBoundingVolumes()
{
	const STile& tile = m_pNavMesh->GetTile(m_tileId);
	if (m_tileId.IsValid() && m_triangleIdxTile + 1 < tile.GetBVNodesCount())
	{
		const Tile::SBVNode& node = tile.GetBVNodes()[m_triangleIdxTile];
		m_triangleIdxTile += node.leaf ? 1 : node.offset;
		return true;
	}
	return false;
}

CNavMeshQuery::CTriangleIterator::STriangleInfo CNavMeshQuery::CTriangleIterator::GetTriangleBoundingVolumes() const
{
	IF_UNLIKELY(!m_tileId.IsValid())
	{
		return CTriangleIterator::STriangleInfo();
	}

	const STile& tile = m_pNavMesh->GetTile(m_tileId);
	IF_UNLIKELY(tile.GetBVNodesCount() < m_triangleIdxTile)
	{
		return CTriangleIterator::STriangleInfo();

	}

	const Tile::STriangle& triangle = tile.GetTriangles()[m_triangleIdxTile];
	const TriangleID triangleId = ComputeTriangleID(m_tileId, m_triangleIdxTile);

	return STriangleInfo(m_tileId, triangleId, m_triangleIdxTile, triangle);
}

//====================================================================
// CNavMeshQuery
//====================================================================

CNavMeshQuery::CNavMeshQuery(const NavMeshQueryId queryId, const SNavMeshQueryConfig& queryConfig)
	: m_queryId(queryId)
	, m_queryConfig(queryConfig)
	, m_status(INavMeshQuery::EQueryStatus::Uninitialized)
	, m_triangleIterator()
#ifdef NAV_MESH_QUERY_DEBUG
	, m_batchCount(0)
	, m_queryDebug(*this)
#endif // NAV_MESH_QUERY_DEBUG
{
	// Empty
}

NavMeshQueryId CNavMeshQuery::GetId() const
{
	return m_queryId;
}

const INavMeshQuery::SNavMeshQueryConfig& CNavMeshQuery::GetQueryConfig() const
{
	return m_queryConfig;
}

INavMeshQuery::EQueryStatus CNavMeshQuery::GetStatus() const
{
	return m_status;
}

void CNavMeshQuery::SetStatus(const INavMeshQuery::EQueryStatus status)
{
	m_status = status;
}

bool CNavMeshQuery::IsValid() const
{
	CRY_ASSERT_MESSAGE(static_cast<int>(MNM::INavMeshQuery::EQueryStatus::Count) == 8, "CNavMeshQuery::IsValid(): A new Enum MNM::INavMeshQuery::EQueryStatus has been added. Make sure it is being handled in the IsValid() function");

	return m_status != MNM::INavMeshQuery::EQueryStatus::Invalid_InitializationFailed &&
		m_status != MNM::INavMeshQuery::EQueryStatus::Invalid_NavMeshAnnotationChanged &&
		m_status != MNM::INavMeshQuery::EQueryStatus::Invalid_NavMeshRegenerated &&
		m_status != MNM::INavMeshQuery::EQueryStatus::Invalid_CachedNavMeshIdNotFound;
}

bool CNavMeshQuery::IsDone() const
{
	CRY_ASSERT_MESSAGE(static_cast<int>(MNM::INavMeshQuery::EQueryStatus::Count) == 8, "CNavMeshQuery::IsDone(): A new Enum MNM::INavMeshQuery::EQueryStatus has been added. Make sure it is being handled in the IsDone() function");

	return m_status == MNM::INavMeshQuery::EQueryStatus::Done_QueryProcessingStopped || 
		m_status == MNM::INavMeshQuery::EQueryStatus::Done_Complete;
}

bool CNavMeshQuery::IsRunning() const
{
	CRY_ASSERT_MESSAGE(static_cast<int>(MNM::INavMeshQuery::EQueryStatus::Count) == 8, "CNavMeshQuery::IsRunning(): A new Enum MNM::INavMeshQuery::EQueryStatus has been added. Make sure it is being handled in the IsRunning() function");
	return m_status == MNM::INavMeshQuery::EQueryStatus::Running;
}

#ifdef NAV_MESH_QUERY_DEBUG
const INavMeshQueryDebug& CNavMeshQuery::GetQueryDebug() const
{
	return m_queryDebug;
}

bool CNavMeshQuery::operator< (const INavMeshQuery & other) const
{
	return this->GetId() < other.GetId();
}

#endif // NAV_MESH_QUERY_DEBUG

bool CNavMeshQuery::Initialize()
{
	m_triangleIterator = CTriangleIterator(m_queryConfig.meshId, m_queryConfig.aabb);

	IF_UNLIKELY (m_triangleIterator.GetNavMesh() == nullptr)
	{
		return false;
	}

	return true;
}

INavMeshQuery::EQueryStatus CNavMeshQuery::Run(INavMeshQueryProcessing& queryProcessing, const size_t processingBatchSize /* = 32*/)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#ifdef NAV_MESH_QUERY_DEBUG
	const CTimeValue timeAtStart = gEnv->pTimer->GetAsyncCurTime();
#endif // NAV_MESH_QUERY_DEBUG

	if (m_status == INavMeshQuery::EQueryStatus::Uninitialized)
	{
#ifdef NAV_MESH_QUERY_DEBUG
		DebugDrawQueryConfig();
#endif // NAV_MESH_QUERY_DEBUG
		if (Initialize())
			SetStatus(INavMeshQuery::EQueryStatus::Running);
		else
			SetStatus(INavMeshQuery::EQueryStatus::Invalid_InitializationFailed);
	}

	if (m_status == INavMeshQuery::EQueryStatus::Running)
	{
#ifdef NAV_MESH_QUERY_DEBUG
		m_trianglesDebug.resize(0);
#endif // NAV_MESH_QUERY_DEBUG
		if (m_triangleIterator.UpdateNavMeshPointer())
			SetStatus(QueryTiles(queryProcessing, processingBatchSize));
		else
			SetStatus(INavMeshQuery::EQueryStatus::Invalid_CachedNavMeshIdNotFound);
#ifdef NAV_MESH_QUERY_DEBUG
		DebugQueryResult(timeAtStart);
#endif // NAV_MESH_QUERY_DEBUG
	}

	return m_status;
}

	
#ifdef NAV_MESH_QUERY_DEBUG
void CNavMeshQuery::DebugQueryResult(const CTimeValue& timeAtStart)
{
	const CTimeValue timeAtEnd = gEnv->pTimer->GetAsyncCurTime();
	const float elapsedTimeInMs = timeAtEnd.GetDifferenceInSeconds(timeAtStart) * 1000;
	const INavMeshQueryDebug::SBatchData queryBatch(m_batchCount++, m_trianglesDebug.size(), elapsedTimeInMs, m_triangleIterator.GetNavMesh(), m_trianglesDebug);
	m_queryDebug.AddBatchToHistory(queryBatch);
	DebugDrawQueryBatch(queryBatch);
}

void CNavMeshQuery::DebugDrawQueryConfig() const
{
	if (!gAIEnv.CVars.navigation.DebugDrawNavigationQueriesUDR)
	{
		return;
	}

	Cry::UDR::CScope_FixedString navMeshQueriesScopes("NavMeshQueries");
	Cry::UDR::CScope_FixedString queriesScope("Queries");
	Cry::UDR::CScope_FixedString callerScope(m_queryConfig.szCallerName);
	{
		stack_string queryIdText;
		queryIdText.Format("Query %zu", m_queryId);
		Cry::UDR::CScope_FixedString queryScope(queryIdText.c_str());
		{
			m_queryConfig.DebugDraw();
		}
	}
}

void CNavMeshQuery::DebugDrawQueryBatch(const MNM::INavMeshQueryDebug::SBatchData& queryBatch) const
{
	if (!gAIEnv.CVars.navigation.DebugDrawNavigationQueriesUDR)
	{
		return;
	}

	Cry::UDR::CScope_FixedString navMeshQueriesScopes("NavMeshQueries");
	{
		{
			Cry::UDR::CScope_FixedString queriesScope("Queries");
			Cry::UDR::CScope_FixedString callerScope(m_queryConfig.szCallerName);
			{
				{
					stack_string queryIdText;
					queryIdText.Format("Query %zu", m_queryId);
					Cry::UDR::CScope_FixedString queryScope(queryIdText.c_str());
					queryBatch.DebugDraw();
				}
			}
		}

		{
			Cry::UDR::CScope_FixedString statisticsScope("Statistics");
			stack_string itemText;
			itemText.Format("Id/b: %lu/%lu \tSize: %lu \tTime: %2.5f ms \tCaller: %s", m_queryId, queryBatch.batchNumber, queryBatch.triangleDataArray.size(), queryBatch.elapsedTimeInMs, m_queryConfig.szCallerName);
			Cry::UDR::CScope_FixedString itemScope(itemText.c_str());
		}
	}

}

void CNavMeshQuery::DebugDrawQueryInvalidation(const MNM::INavMeshQueryDebug::SInvalidationData& invalidationData) const
{
	if (!gAIEnv.CVars.navigation.DebugDrawNavigationQueriesUDR)
	{
		return;
	}

	Cry::UDR::CScope_FixedString queriesScope("Queries");
	Cry::UDR::CScope_FixedString navMeshQueriesScopes("NavMeshQueries");
	Cry::UDR::CScope_FixedString callerScope(m_queryConfig.szCallerName);
	{
		stack_string queryIdText;
		queryIdText.Format("Query %zu", m_queryId);
		Cry::UDR::CScope_FixedString queryScope(queryIdText.c_str());
		{
			invalidationData.DebugDraw();
		}
	}
}
#endif // NAV_MESH_QUERY_DEBUG

MNM::aabb_t CNavMeshQuery::CaculateQueryAabbInTileSpace(TileID tileId) const
{
	const CNavMesh::SGridParams gridParams = m_triangleIterator.GetNavMesh()->GetGridParams();

	const vector3_t tileMin(0, 0, 0);
	const vector3_t tileMax(gridParams.tileSize.x, gridParams.tileSize.y, gridParams.tileSize.z);

	Tile::STileData tileData;
	m_triangleIterator.GetNavMesh()->GetTileData(tileId, tileData);

	const aabb_t queryAabb(
		vector3_t::maximize(m_queryConfig.aabb.min - tileData.tileOriginWorld, tileMin),
		vector3_t::minimize(m_queryConfig.aabb.max - tileData.tileOriginWorld, tileMax)
	);
	return queryAabb;
}

bool CNavMeshQuery::TriangleIntersectsQueryAabb(const MNM::aabb_t& queryAabbInTileSpace, const CTriangleIterator::STriangleInfo& triangleInfo) const
{
	const STile tile = m_triangleIterator.GetNavMesh()->GetTile(triangleInfo.tileId);

	const MNM::vector3_t v0 = tile.GetVertices()[triangleInfo.triangle.vertex[0]];
	const MNM::vector3_t v1 = tile.GetVertices()[triangleInfo.triangle.vertex[1]];
	const MNM::vector3_t v2 = tile.GetVertices()[triangleInfo.triangle.vertex[2]];

	const MNM::aabb_t triangleAabb(MNM::vector3_t::minimize(v0, v1, v2), MNM::vector3_t::maximize(v0, v1, v2));

	bool result = false;
	switch (m_queryConfig.overlappingMode)
	{
	case MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial:
		result = queryAabbInTileSpace.overlaps(triangleAabb);
		break;
	case MNM::ENavMeshQueryOverlappingMode::BoundingBox_Full:
		result = queryAabbInTileSpace.contains(triangleAabb);
		break;
	case MNM::ENavMeshQueryOverlappingMode::Triangle_Partial:
		result = queryAabbInTileSpace.overlaps(v0, v1, v2);
		break;
	case MNM::ENavMeshQueryOverlappingMode::Triangle_Full:
		result = queryAabbInTileSpace.contains(v0) && queryAabbInTileSpace.contains(v1) && queryAabbInTileSpace.contains(v2);
		break;
	default:
		CRY_ASSERT_MESSAGE(false, "CNavMeshQuery::TriangleIntersectsQueryAabb: New enum value has been added to MNM::INavMeshQuery::SNavMeshQueryConfig::IntersectionMode but it's not being handled in this switch statement");
	}
	return result;
}

//====================================================================
// CNavMeshQueryInstant
//====================================================================

CNavMeshQueryInstant::CNavMeshQueryInstant(const NavMeshQueryId queryId, const INavMeshQuery::SNavMeshQueryConfigInstant& queryConfig)
	: BaseClass(queryId, m_queryConfig)
	, m_queryConfig(queryConfig)
{
	// Empty
}

INavMeshQuery::EQueryStatus CNavMeshQueryInstant::QueryTiles(INavMeshQueryProcessing& queryProcessing, const size_t processingBatchSize)
{
	if (m_queryConfig.pQueryFilter)
	{
		return QueryTilesInternal(queryProcessing, *m_queryConfig.pQueryFilter, processingBatchSize);
	}
	else
	{
		return QueryTilesInternal(queryProcessing, DefaultQueryFilters::g_globalFilter, processingBatchSize);
	}
}

template<typename TFilter>
INavMeshQuery::EQueryStatus CNavMeshQueryInstant::QueryTilesInternal(INavMeshQueryProcessing& queryProcessing, const TFilter& filter, const size_t processingBatchSize)
{
	aabb_t cachedQueryAabbInTileSpace;
	INavMeshQueryProcessing::EResult queryProcessingResult;

	TileID cachedTileId = TileID();
	bool queryProcessingStopped = false;

	while (m_triangleIterator.HasNext() && !queryProcessingStopped)
	{
		const CTriangleIterator::STriangleInfo triangleInfo = m_triangleIterator.GetTriangle();
		if (triangleInfo.IsValid()) 
		{
			if (cachedTileId != triangleInfo.tileId)
			{
				cachedTileId = triangleInfo.tileId;
				cachedQueryAabbInTileSpace = CaculateQueryAabbInTileSpace(cachedTileId);
			}

			if (filter.PassFilter(triangleInfo.triangle) && TriangleIntersectsQueryAabb(cachedQueryAabbInTileSpace, triangleInfo))
			{
				queryProcessing.AddTriangle(triangleInfo.triangleId);
#ifdef NAV_MESH_QUERY_DEBUG
				m_trianglesDebug.push_back(triangleInfo.triangleId);
#endif // NAV_MESH_QUERY_DEBUG

				if (queryProcessing.Size() == processingBatchSize)
				{
					queryProcessingResult = queryProcessing.Process();
					queryProcessingStopped = queryProcessingResult == INavMeshQueryProcessing::EResult::Stop;
				}
			}
		}

		m_triangleIterator.Next();
	}

	if (queryProcessing.Size() > 0)
	{
		queryProcessingResult = queryProcessing.Process();
	}

	if (queryProcessingResult == INavMeshQueryProcessing::EResult::Stop)
	{
		return INavMeshQuery::EQueryStatus::Done_QueryProcessingStopped;
	}
	else
	{
		return INavMeshQuery::EQueryStatus::Done_Complete;
	}
}

//====================================================================
// CNavMeshQueryBatch
//====================================================================

CNavMeshQueryBatch::CNavMeshQueryBatch(const NavMeshQueryId queryId, const INavMeshQuery::SNavMeshQueryConfigBatch& queryConfig)
	: BaseClass(queryId, m_queryConfig)
	, m_queryConfig(queryConfig)
{
	InitializeRuntimeData();
	RegisterNavMeshCallbacks();
}

CNavMeshQueryBatch::~CNavMeshQueryBatch()
{
	if (!IsDone())
	{
		DeregisterNavMeshCallbacks();
	}
}

void CNavMeshQueryBatch::InitializeRuntimeData()
{
	m_runtimeData.navAgentTypeId = gAIEnv.pNavigationSystem->GetAgentTypeOfMesh(m_queryConfig.meshId);
	m_runtimeData.processedTilesId.reserve(m_queryConfig.processingTrianglesMaxSize);
}

INavMeshQuery::EQueryStatus CNavMeshQueryBatch::QueryTiles(INavMeshQueryProcessing& queryProcessing, const size_t processingBatchSize)
{
	queryProcessing.Clear();
	if (m_queryConfig.pQueryFilter)
	{
		return QueryTilesInternal(queryProcessing, *m_queryConfig.pQueryFilter, processingBatchSize);
	}
	else
	{
		return QueryTilesInternal(queryProcessing, DefaultQueryFilters::g_globalFilter, processingBatchSize);
	}
}

template<typename TFilter>
INavMeshQuery::EQueryStatus CNavMeshQueryBatch::QueryTilesInternal(INavMeshQueryProcessing& queryProcessing, const TFilter& filter, const size_t processingBatchSize)
{
	aabb_t cachedQueryAabbInTileSpace;
	INavMeshQueryProcessing::EResult queryProcessingResult;

	size_t remainingTrianglesCount = m_queryConfig.processingTrianglesMaxSize;
	TileID cachedTileId = TileID();
	bool queryProcessingStopped = false;

	while (m_triangleIterator.HasNext() && !queryProcessingStopped && remainingTrianglesCount > 0 )
	{
		const CTriangleIterator::STriangleInfo triangleInfo = m_triangleIterator.GetTriangle();
		if (triangleInfo.IsValid())
		{
			if (cachedTileId != triangleInfo.tileId)
			{
				cachedTileId = triangleInfo.tileId;
				cachedQueryAabbInTileSpace = CaculateQueryAabbInTileSpace(cachedTileId);
			}

			if (filter.PassFilter(triangleInfo.triangle) && TriangleIntersectsQueryAabb(cachedQueryAabbInTileSpace, triangleInfo))
			{
				queryProcessing.AddTriangle(triangleInfo.triangleId);
#ifdef NAV_MESH_QUERY_DEBUG
				m_trianglesDebug.push_back(triangleInfo.triangleId);
#endif // NAV_MESH_QUERY_DEBUG
				--remainingTrianglesCount;

				if (queryProcessing.Size() == processingBatchSize)
				{
					queryProcessingResult = queryProcessing.Process();
					queryProcessingStopped = queryProcessingResult == INavMeshQueryProcessing::EResult::Stop;
				}
			}
		}
		m_triangleIterator.Next();
	}

	if (queryProcessing.Size() > 0)
	{
		queryProcessingResult = queryProcessing.Process();
	}

	if (queryProcessingResult == INavMeshQueryProcessing::EResult::Stop || !m_triangleIterator.HasNext())
	{
		DeregisterNavMeshCallbacks();
		if (queryProcessingResult == INavMeshQueryProcessing::EResult::Stop)
		{
			return INavMeshQuery::EQueryStatus::Done_QueryProcessingStopped;
		}
		else
		{
			return INavMeshQuery::EQueryStatus::Done_Complete;
		}
	}
	else
	{
		return INavMeshQuery::EQueryStatus::Running;

	}
}

void CNavMeshQueryBatch::RegisterNavMeshCallbacks()
{
	gAIEnv.pNavigationSystem->AddMeshChangeCallback(m_runtimeData.navAgentTypeId, functor(*this, &CNavMeshQueryBatch::OnNavMeshChanged));
	gAIEnv.pNavigationSystem->AddMeshAnnotationChangeCallback(m_runtimeData.navAgentTypeId, functor(*this, &CNavMeshQueryBatch::OnNavMeshAnnotationChanged));
}

void CNavMeshQueryBatch::DeregisterNavMeshCallbacks()
{
	gAIEnv.pNavigationSystem->RemoveMeshChangeCallback(m_runtimeData.navAgentTypeId, functor(*this, &CNavMeshQueryBatch::OnNavMeshChanged));
	gAIEnv.pNavigationSystem->RemoveMeshAnnotationChangeCallback(m_runtimeData.navAgentTypeId, functor(*this, &CNavMeshQueryBatch::OnNavMeshAnnotationChanged));
}

void CNavMeshQueryBatch::OnNavMeshChanged(NavigationAgentTypeID navAgentId, NavigationMeshID navMeshId, TileID changedTileId)
{
	CRY_ASSERT_MESSAGE(!IsDone(), "CNavMeshQueryBatch::OnNavMeshChanged: This shouldn't happen. When a query is done, it should automatically be de-registered from NavMesh changes.");

	if (m_queryConfig.meshId != navMeshId || m_runtimeData.navAgentTypeId != navAgentId)
	{
		return;
	}

#ifdef NAV_MESH_QUERY_DEBUG
	bool regenerationAffectsQuery = false;
#endif //NAV_MESH_QUERY_DEBUG

	switch (m_queryConfig.actionOnNavMeshRegeneration)
	{
	case INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::InvalidateAlways:
#ifdef NAV_MESH_QUERY_DEBUG
		regenerationAffectsQuery = true;
#endif //NAV_MESH_QUERY_DEBUG
		SetStatus(INavMeshQuery::EQueryStatus::Invalid_NavMeshRegenerated);
		break;
	case INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::InvalidateIfProccesed:
		if (IsTileProcessed(changedTileId))
		{
#ifdef NAV_MESH_QUERY_DEBUG
			regenerationAffectsQuery = true;
#endif //NAV_MESH_QUERY_DEBUG
			SetStatus(INavMeshQuery::EQueryStatus::Invalid_NavMeshRegenerated);
		}
		break;
	case INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::Ignore:
	{
		const CTriangleIterator::STriangleInfo triangleInfo = m_triangleIterator.GetTriangle();
		if (triangleInfo.IsValid() && triangleInfo.triangleIdx > 0 && triangleInfo.tileId == changedTileId)
		{
			SetStatus(INavMeshQuery::EQueryStatus::Invalid_NavMeshRegenerated);
		}
		break;
	}
	default:
		CRY_ASSERT_MESSAGE(false, "CNavMeshQueryBatch::OnNavMeshChanged: New enum value has been added to MNM::INavMeshQuery::SNavMeshQueryConfig::EActionOnNavMeshChange but it's not being handled in this switch statement");
	}

#ifdef NAV_MESH_QUERY_DEBUG
	if (regenerationAffectsQuery)
	{
		DebugInvalidation(changedTileId, INavMeshQueryDebug::EInvalidationType::NavMeshRegeneration);
	}
#endif //NAV_MESH_QUERY_DEBUG
}

void CNavMeshQueryBatch::OnNavMeshAnnotationChanged(NavigationAgentTypeID navAgentId, NavigationMeshID navMeshId, TileID changedTileId)
{
	CRY_ASSERT_MESSAGE(!IsDone(), "CNavMeshQueryBatch::OnNavMeshAnnotationChanged: This shouldn't happen. When a query is done, it should automatically be de-registered from NavMesh annotation changes.");

	if (m_queryConfig.meshId != navMeshId || m_runtimeData.navAgentTypeId == navAgentId)
	{
		return;
	}

#ifdef NAV_MESH_QUERY_DEBUG
	bool changeAffectsQuery = false;
#endif //NAV_MESH_QUERY_DEBUG

	switch (m_queryConfig.actionOnNavMeshRegeneration)
	{
	case INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::InvalidateAlways:
#ifdef NAV_MESH_QUERY_DEBUG
		changeAffectsQuery = true;
#endif //NAV_MESH_QUERY_DEBUG
		SetStatus(INavMeshQuery::EQueryStatus::Invalid_NavMeshAnnotationChanged);
		break;
	case INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::InvalidateIfProccesed:
		if (IsTileProcessed(changedTileId))
		{
#ifdef NAV_MESH_QUERY_DEBUG
			changeAffectsQuery = true;
#endif //NAV_MESH_QUERY_DEBUG
			SetStatus(INavMeshQuery::EQueryStatus::Invalid_NavMeshAnnotationChanged);
		}
		break;
	case INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::Ignore:
	{
		const CTriangleIterator::STriangleInfo triangleInfo = m_triangleIterator.GetTriangle();
		if (triangleInfo.IsValid() && triangleInfo.triangleIdx > 0 && triangleInfo.tileId == changedTileId)
		{
			SetStatus(INavMeshQuery::EQueryStatus::Invalid_NavMeshAnnotationChanged);
		}
		break;
	}
	default:
		CRY_ASSERT_MESSAGE(false, "CNavMeshQueryBatch::OnNavMeshAnnotationChanged: New enum value has been added to MNM::INavMeshQuery::SNavMeshQueryConfig::EActionOnNavMeshChange but it's not being handled in this switch statement");
	}

#ifdef NAV_MESH_QUERY_DEBUG
	if (changeAffectsQuery)
	{
		DebugInvalidation(changedTileId, INavMeshQueryDebug::EInvalidationType::NavMeshAnnotationChanged);
	}
#endif //NAV_MESH_QUERY_DEBUG

}

bool CNavMeshQueryBatch::IsTileProcessed(const TileID tileId) const
{
	TilesID::const_iterator it = m_runtimeData.processedTilesId.find(tileId);
	return it != m_runtimeData.processedTilesId.end();
}

void CNavMeshQueryBatch::SetTileProcessed(const TileID tileId)
{
	m_runtimeData.processedTilesId.insert(tileId);
}

#ifdef NAV_MESH_QUERY_DEBUG
void CNavMeshQueryBatch::DebugInvalidation(const TileID tileId, const INavMeshQueryDebug::EInvalidationType invalidationType)
{
	MNM::Tile::STileData tileData;
	m_triangleIterator.GetNavMesh()->GetTileData(tileId, tileData);
	const INavMeshQueryDebug::SInvalidationData queryInvalidation(tileId, tileData, invalidationType);
	m_queryDebug.AddInvalidationToHistory(queryInvalidation);
	DebugDrawQueryInvalidation(queryInvalidation);
}
#endif //NAV_MESH_QUERY_DEBUG

} // namespace MNM