// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavMesh.h"
#include "NavMeshQuery.h"
#include "NavMeshQueryManager.h"
#include "NavMeshQueryProcessing.h"
#include "MNMUtils.h"
#include "OffGridLinks.h"
#include "Tile.h"
#include "TileConnectivity.h"
#include "WayQuery.h"

#include "../NavigationSystem/OffMeshNavigationManager.h"
#include "../NavigationSystem/NavigationSystem.h"


#if defined(min)
	#undef min
#endif
#if defined(max)
	#undef max
#endif

// - if this is #defined, then some public methods of CNavMesh::TileContainerArray will receive additional asserts() to debug access to freed TileContainer instances; this might
//   generate false positives while we're in the process of tracing down bugs though
// - if not #defined, then the code will effectively behave the same as before 2015-08-13 (i. e. it will *not* do these extra checks, thus not generating false positives)
//#define TILE_CONTAINER_ARRAY_STRICT_ACCESS_CHECKS

//#pragma optimize("", off)
//#pragma inline_depth(0)

namespace MNM
{

//////////////////////////////////////////////////////////////////////////

bool CNavMesh::SWayQueryRequest::CanUseOffMeshLink(const IOffMeshLink* pOffMeshLink, float* costMultiplier) const
{
	if (m_requesterEntityId)
	{
		if (pOffMeshLink)
		{
			return pOffMeshLink->CanUse(m_requesterEntityId, costMultiplier);
		}
	}
	return true;    // Always allow by default
}

const IOffMeshLink* CNavMesh::SWayQueryRequest::GetOffMeshLinkAndAnnotation(const OffMeshLinkID linkId, MNM::AreaAnnotation& annotation) const
{
	return m_offMeshNavigationManager.GetLinkAndAnnotation(linkId, annotation);
}

//////////////////////////////////////////////////////////////////////////

CNavMesh::TileContainerArray::TileContainerArray()
	: m_tiles(NULL)
	, m_tileCount(0)
	, m_tileCapacity(0)
{
}

CNavMesh::TileContainerArray::TileContainerArray(const TileContainerArray& rhs)
	: m_tiles(NULL)
	, m_tileCount(0)
	, m_tileCapacity(0)
{
	// if this assert() triggers, then we must completely re-think the use-cases in which copy-construction can occur
	assert(rhs.m_tiles == NULL);
}

CNavMesh::TileContainerArray::~TileContainerArray()
{
	for (size_t i = 0; i < m_tileCapacity; ++i)
		m_tiles[i].tile.Destroy();

	delete[] m_tiles;
}

void CNavMesh::TileContainerArray::Grow(size_t amount)
{
	const size_t oldCapacity = m_tileCapacity;

	m_tileCapacity += amount;
	TileContainer* tiles = new TileContainer[m_tileCapacity];

	// careful #1: memcpy'ing is ok as Tile contains only primitive data types and pointers
	// #MNM_TODO pavloi 2016.07.26: can't use std::is_trivially_copyable - unexpectedly it's not implemented for clang/gcc in our build configurations
	//COMPILE_TIME_ASSERT(std::is_trivially_copyable<TileContainer>::value);
	if (oldCapacity)
		memcpy(tiles, m_tiles, oldCapacity * sizeof(TileContainer));

	std::swap(m_tiles, tiles);

	// careful #2: don't worry, Tile has no dtor that would free memory pointed to by some of its members and suddenly leave parts of m_tiles[] dangling
	//             (TileContainerArray::~TileContainerArray() will call Tile::Destroy() to free its memory)
	delete[] tiles;

	// careful #3: we won't fill up m_frees as all elements starting at [m_frees.size() + m_tileCount] are implicitly marked as free.
}

void CNavMesh::TileContainerArray::Init(size_t initialTileContainerCount)
{
	assert(m_tileCapacity == 0);
	assert(m_tiles == NULL);
	Grow(initialTileContainerCount);
}

const TileID CNavMesh::TileContainerArray::AllocateTileContainer()
{
	assert(m_tileCount <= m_tileCapacity);

	++m_tileCount;

	TileID tileID;

	if (m_freeIndexes.empty())
	{
		tileID = TileID(m_tileCount);

		if (m_tileCount > m_tileCapacity)
			Grow(std::max<size_t>(4, m_tileCapacity >> 1));
	}
	else
	{
		tileID = TileID(m_freeIndexes.back() + 1);
		m_freeIndexes.pop_back();
	}

	return tileID;
}

void CNavMesh::TileContainerArray::FreeTileContainer(const TileID tileID)
{
	const size_t index = tileID - 1;

	assert(tileID > 0);
	assert(tileID <= m_tileCapacity);
#ifdef TILE_CONTAINER_ARRAY_STRICT_ACCESS_CHECKS
	assert(!stl::find(m_freeIndexes, index));
	assert(index < m_freeIndexes.size() + m_tileCount);
#endif

	--m_tileCount;
	m_freeIndexes.push_back(index);
}

const size_t CNavMesh::TileContainerArray::GetTileCount() const
{
	return m_tileCount;
}

CNavMesh::TileContainer& CNavMesh::TileContainerArray::operator[](size_t index)
{
	assert(index < m_tileCapacity);
#ifdef TILE_CONTAINER_ARRAY_STRICT_ACCESS_CHECKS
	assert(!stl::find(m_freeIndexes, index));
	assert(index < m_freeIndexes.size() + m_tileCount);
#endif
	return m_tiles[index];
}

const CNavMesh::TileContainer& CNavMesh::TileContainerArray::operator[](size_t index) const
{
	assert(index < m_tileCapacity);
#ifdef TILE_CONTAINER_ARRAY_STRICT_ACCESS_CHECKS
	assert(!stl::find(m_freeIndexes, index));
	assert(index < m_freeIndexes.size() + m_tileCount);
#endif
	return m_tiles[index];
}

void CNavMesh::TileContainerArray::Swap(TileContainerArray& other)
{
	std::swap(m_tiles, other.m_tiles);
	std::swap(m_tileCount, other.m_tileCount);
	std::swap(m_tileCapacity, other.m_tileCapacity);
	m_freeIndexes.swap(other.m_freeIndexes);
}

void CNavMesh::TileContainerArray::UpdateProfiler(ProfilerType& profiler) const
{
	profiler.AddMemory(GridMemory, m_tileCapacity * sizeof(TileContainer));
	profiler.AddMemory(GridMemory, m_freeIndexes.capacity() * sizeof(FreeIndexes::value_type));
}

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
void CNavMesh::TileContainerArray::BreakOnInvalidTriangle(const TriangleID triangleID) const
{
	const TileID checkTileID = ComputeTileID(triangleID);
	const size_t index = checkTileID - 1;

	if (checkTileID == 0)
	{
		__debugbreak();
	}
	else if (checkTileID > m_tileCapacity)
	{
		__debugbreak();
	}
	#ifdef TILE_CONTAINER_ARRAY_STRICT_ACCESS_CHECKS
	else if (stl::find(m_freeIndexes, index))
	{
		__debugbreak();
	}
	else if (index >= m_freeIndexes.size() + m_tileCount)
	{
		__debugbreak();
	}
	#endif // TILE_CONTAINER_ARRAY_STRICT_ACCESS_CHECKS
}
#endif

const real_t CNavMesh::kMinPullingThreshold = real_t(0.05f);
const real_t CNavMesh::kMaxPullingThreshold = real_t(0.95f);
const real_t CNavMesh::kAdjecencyCalculationToleranceSq = square(real_t(0.02f));

CNavMesh::CNavMesh()
	: m_triangleCount(0)
{
}

CNavMesh::~CNavMesh()
{
}

void CNavMesh::Init(const NavigationMeshID meshId, const SGridParams& params, const SAgentSettings& agentSettings)
{
	m_meshId = meshId;

	m_params = params;
	m_agentSettings = agentSettings;

	m_tiles.Init(params.tileCount);
}

DynArray<TriangleID> CNavMesh::QueryTriangles(const aabb_t& localAabb) const 
{
	return QueryTriangles(localAabb, ENavMeshQueryOverlappingMode::BoundingBox_Partial, nullptr);
}

DynArray<TriangleID> CNavMesh::QueryTriangles(const aabb_t& localAabb, const ENavMeshQueryOverlappingMode overlappingMode, const INavMeshQueryFilter* pFilter) const 
{
	const ::MNM::INavMeshQuery::SNavMeshQueryConfigInstant config(
		m_meshId,
		"CNavMesh::GetTriangles",
		localAabb,
		overlappingMode,
		pFilter
	);

	CDefaultProcessing queryProcessing;
	GetAISystem()->GetNavigationSystem()->GetNavMeshQueryManager()->RunInstantQuery(config, queryProcessing);
	return std::move(queryProcessing.GetTriangleIdArray());
}

TriangleID CNavMesh::QueryTriangleAt(const vector3_t& localPosition, const real_t verticalDownwardRange, const real_t verticalUpwardRange) const 
{
	return QueryTriangleAt(localPosition, verticalDownwardRange, verticalUpwardRange, ENavMeshQueryOverlappingMode::BoundingBox_Partial, nullptr);
}

TriangleID CNavMesh::QueryTriangleAt(const vector3_t& localPosition, const real_t verticalDownwardRange, const real_t verticalUpwardRange, const ENavMeshQueryOverlappingMode overlappingMode, const INavMeshQueryFilter* pFilter) const 
{
	const aabb_t aabb(
		MNM::vector3_t(localPosition.x, localPosition.y, localPosition.z - verticalDownwardRange),
		MNM::vector3_t(localPosition.x, localPosition.y, localPosition.z + verticalUpwardRange)
	);

	const INavMeshQuery::SNavMeshQueryConfigInstant config(
		m_meshId,
		"CNavMesh::QueryTriangleAt",
		aabb,
		overlappingMode,
		pFilter
	);

	CTriangleAtQueryProcessing queryProcessing(m_meshId, localPosition);
	GetAISystem()->GetNavigationSystem()->GetNavMeshQueryManager()->RunInstantQuery(config, queryProcessing);
	return queryProcessing.GetTriangleId();
}

SClosestTriangle CNavMesh::QueryClosestTriangle(const vector3_t& worldPos, const aabb_t& localAabbAroundPosition) const 
{
	return QueryClosestTriangle( worldPos, localAabbAroundPosition, ENavMeshQueryOverlappingMode::BoundingBox_Partial, real_t::max(), nullptr);
}

SClosestTriangle CNavMesh::QueryClosestTriangle(const vector3_t& worldPos, const aabb_t& localAabbAroundPosition, const ENavMeshQueryOverlappingMode overlappingMode, const MNM::real_t maxDistance, const INavMeshQueryFilter* pFilter) const 
{
	const MNM::aabb_t aabb(worldPos + localAabbAroundPosition.min, worldPos + localAabbAroundPosition.max);

	const INavMeshQuery::SNavMeshQueryConfigInstant config(
		m_meshId,
		"CNavMesh::QueryClosestTriangle",
		aabb,
		overlappingMode,
		pFilter
	);

	CNearestTriangleQueryProcessing queryProcessing(m_meshId, worldPos, maxDistance);
	GetAISystem()->GetNavigationSystem()->GetNavMeshQueryManager()->RunInstantQuery(config, queryProcessing);

	return MNM::SClosestTriangle(queryProcessing.GetClosestTriangleId(),
		queryProcessing.GetClosestPosition(),
		queryProcessing.GetClosestDistance());
}

bool CNavMesh::QueryIsTriangleAcceptableForLocation(const vector3_t& localPosition, const TriangleID triangleID) const 
{
	return QueryIsTriangleAcceptableForLocation(localPosition, triangleID, real_t(1.0f), ENavMeshQueryOverlappingMode::BoundingBox_Partial, nullptr);
}

bool CNavMesh::QueryIsTriangleAcceptableForLocation(const vector3_t& localPosition, const TriangleID triangleID, const real_t range, const ENavMeshQueryOverlappingMode overlappingMode, const INavMeshQueryFilter* pFilter) const 
{
	const MNM::aabb_t aabb(
		MNM::vector3_t(localPosition.x - range, localPosition.y - range, localPosition.z - range),
		MNM::vector3_t(localPosition.x + range, localPosition.y + range, localPosition.z + range));

	const INavMeshQuery::SNavMeshQueryConfigInstant config(
		m_meshId,
		"CNavMesh::QueryIsTriangleAcceptableForLocation",
		aabb,
		overlappingMode,
		pFilter
	);

	CFindTriangleQueryProcessing queryProcessing(m_meshId, triangleID, localPosition);
	GetAISystem()->GetNavigationSystem()->GetNavMeshQueryManager()->RunInstantQuery(config, queryProcessing);
	return queryProcessing.WasTriangleFound();
}

INavigationSystem::NavMeshBorderWithNormalArray CNavMesh::QueryMeshBorders(const aabb_t& localAabb) const 
{
	return QueryMeshBorders(localAabb, ENavMeshQueryOverlappingMode::BoundingBox_Partial, nullptr, nullptr);
}

INavigationSystem::NavMeshBorderWithNormalArray CNavMesh::QueryMeshBorders(const aabb_t& localAabb, ENavMeshQueryOverlappingMode overlappingMode, const INavMeshQueryFilter* pQueryFilter, const INavMeshQueryFilter* pAnnotationFilter) const 
{
	const INavMeshQuery::SNavMeshQueryConfigInstant config(
		m_meshId,
		"CNavMesh::QueryMeshBorders",
		localAabb,
		overlappingMode,
		pQueryFilter
	);

	CGetMeshBordersQueryProcessing queryProcessing(m_meshId, pAnnotationFilter ? *pAnnotationFilter : DefaultQueryFilters::g_globalFilterVirtual);
	GetAISystem()->GetNavigationSystem()->GetNavMeshQueryManager()->RunInstantQuery(config, queryProcessing);
	return std::move(queryProcessing.GetBordersNormals());
}

bool CNavMesh::GetVertices(TriangleID triangleID, vector3_t& v0, vector3_t& v1, vector3_t& v2) const
{
	if (const TileID tileID = ComputeTileID(triangleID))
	{
		const TileContainer& container = m_tiles[tileID - 1];

		const vector3_t tileOrigin = GetTileOrigin(container.x, container.y, container.z);

		const Tile::STriangle& triangle = container.tile.triangles[ComputeTriangleIndex(triangleID)];
		v0 = tileOrigin + vector3_t(container.tile.vertices[triangle.vertex[0]]);
		v1 = tileOrigin + vector3_t(container.tile.vertices[triangle.vertex[1]]);
		v2 = tileOrigin + vector3_t(container.tile.vertices[triangle.vertex[2]]);

		return true;
	}

	return false;
}

bool CNavMesh::GetVertices(TriangleID triangleID, vector3_t* verts) const
{
	return GetVertices(triangleID, verts[0], verts[1], verts[2]);
}

bool CNavMesh::GetLinkedEdges(TriangleID triangleID, size_t& linkedEdges) const
{
	if (const TileID tileID = ComputeTileID(triangleID))
	{
		const TileContainer& container = m_tiles[tileID - 1];
		const Tile::STriangle& triangle = container.tile.triangles[ComputeTriangleIndex(triangleID)];

		linkedEdges = 0;

		for (size_t l = 0; l < triangle.linkCount; ++l)
		{
			const Tile::SLink& link = container.tile.links[triangle.firstLink + l];
			const size_t edge = link.edge;
			linkedEdges |= static_cast<size_t>(1) << edge;
		}

		return true;
	}

	return false;
}

bool CNavMesh::GetTriangle(TriangleID triangleID, Tile::STriangle& triangle) const
{
	if (const TileID tileID = ComputeTileID(triangleID))
	{
		const TileContainer& container = m_tiles[tileID - 1];
		triangle = container.tile.triangles[ComputeTriangleIndex(triangleID)];

		return true;
	}

	return false;
}

const AreaAnnotation* CNavMesh::GetTriangleAnnotation(TriangleID triangleID) const
{
	if (const TileID tileID = ComputeTileID(triangleID))
	{
		const TileContainer& container = m_tiles[tileID - 1];
		const uint16 triIndex = ComputeTriangleIndex(triangleID);
		CRY_ASSERT(triIndex < container.tile.triangleCount);
		Tile::STriangle& triangle = container.tile.triangles[triIndex];
		return &triangle.areaAnnotation;
	}
	return nullptr;
}

void CNavMesh::SetTrianglesAnnotation(const TriangleID* pTrianglesArray, const size_t trianglesCount, const MNM::AreaAnnotation areaAnnotation, std::vector<TriangleID>& changedTriangles)
{
	for (size_t i = 0; i < trianglesCount; ++i)
	{
		const TriangleID triangleId = pTrianglesArray[i];
		if (const TileID tileId = MNM::ComputeTileID(triangleId))
		{
			const uint16 triangleIndex = MNM::ComputeTriangleIndex(triangleId);
			MNM::Tile::STriangle& triangle = GetTriangleUnsafe(tileId, triangleIndex);
			if (triangle.areaAnnotation == areaAnnotation)
				continue;

			triangle.areaAnnotation = areaAnnotation;

			changedTriangles.push_back(triangleId);
		}
	}
}

bool CNavMesh::CanTrianglePassFilter(const TriangleID triangleID, const INavMeshQueryFilter& filter) const
{
	if (const TileID tileID = ComputeTileID(triangleID))
	{
		const TileContainer& container = m_tiles[tileID - 1];
		Tile::STriangle& triangle = container.tile.triangles[ComputeTriangleIndex(triangleID)];
		return filter.PassFilter(triangle);
	}
	return false;
}

bool CNavMesh::SnapPosition(
	const vector3_t& localPosition, const SOrderedSnappingMetrics& snappingMetrics, const INavMeshQueryFilter* pFilter,
	vector3_t* pSnappedLocalPosition, TriangleID* pTriangleId) const
{
	const MNM::real_t verticalDefaultDownRange = MNM::Utils::CalculateMinVerticalRange(m_agentSettings.height, m_params.voxelSize.z);
	const MNM::real_t verticalDefaultUpRange = MNM::real_t(min(2u, uint32(m_agentSettings.height)) * m_params.voxelSize.z);
	const MNM::real_t horizontalDefaultRange = MNM::Utils::CalculateMinHorizontalRange(m_agentSettings.radius, m_params.voxelSize.x);

	for (const SSnappingMetric& snappingMetric : snappingMetrics.metricsArray)
	{
		const MNM::real_t verticalDownRange = snappingMetric.verticalDownRange == -FLT_MAX ? verticalDefaultDownRange : MNM::real_t(snappingMetric.verticalDownRange);
		const MNM::real_t verticalUpRange = snappingMetric.verticalUpRange == -FLT_MAX ? verticalDefaultUpRange : MNM::real_t(snappingMetric.verticalUpRange);

		switch (snappingMetric.type)
		{
		case SSnappingMetric::EType::Vertical:
		{
			if (const TriangleID closestId = QueryTriangleAt(localPosition, verticalDownRange, verticalUpRange, ENavMeshQueryOverlappingMode::BoundingBox_Partial, pFilter))
			{
				if (pTriangleId)
					*pTriangleId = closestId;
				
				if (pSnappedLocalPosition)
				{
					MNM::vector3_t vertices[3];
					GetVertices(closestId, vertices);
					if (!Utils::ProjectPointOnTriangleVertical(localPosition, vertices[0], vertices[1], vertices[2], *pSnappedLocalPosition))
					{
						break;
					}
				}
				return true;
			}
			break;
		}
		case SSnappingMetric::EType::Box:
		case SSnappingMetric::EType::Circular:
		{
			const MNM::real_t horizontalRange = snappingMetric.horizontalRange == -FLT_MAX ? horizontalDefaultRange : MNM::real_t(snappingMetric.horizontalRange);
			const MNM::aabb_t localAabb(MNM::vector3_t(-horizontalRange, -horizontalRange, -verticalDownRange), MNM::vector3_t(horizontalRange, horizontalRange, verticalUpRange));
			const MNM::real_t maxDistance = snappingMetric.type == SSnappingMetric::EType::Circular ? horizontalRange : real_t::max();
			const MNM::SClosestTriangle closestTriangle = QueryClosestTriangle(localPosition, localAabb, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, maxDistance, pFilter);
			if (closestTriangle.id.IsValid())
			{
				if (pSnappedLocalPosition)
					*pSnappedLocalPosition = closestTriangle.position;
				if (pTriangleId)
					*pTriangleId = closestTriangle.id;
				return true;
			}
			break;
		}
		default:
			CRY_ASSERT_MESSAGE(false, "CNavMesh::SnapPosition: Unhandled snap metric type!");
			break;
		}
	}
	return false;
}

bool CNavMesh::SnapPosition(
	const vector3_t& localPosition, const SSnappingMetric& snappingMetric, const INavMeshQueryFilter* pFilter,
	vector3_t* pSnappedLocalPosition, TriangleID* pTriangleId) const
{
	const MNM::real_t verticalDefaultDownRange = MNM::Utils::CalculateMinVerticalRange(m_agentSettings.height, m_params.voxelSize.z);
	const MNM::real_t verticalDefaultUpRange = MNM::real_t(min(2u, uint32(m_agentSettings.height)) * m_params.voxelSize.z);
	const MNM::real_t horizontalDefaultRange = MNM::Utils::CalculateMinHorizontalRange(m_agentSettings.radius, m_params.voxelSize.x);

	const MNM::real_t verticalDownRange = snappingMetric.verticalDownRange == -FLT_MAX ? verticalDefaultDownRange : MNM::real_t(snappingMetric.verticalDownRange);
	const MNM::real_t verticalUpRange = snappingMetric.verticalUpRange == -FLT_MAX ? verticalDefaultUpRange : MNM::real_t(snappingMetric.verticalUpRange);

	switch (snappingMetric.type)
	{
	case SSnappingMetric::EType::Vertical:
	{
		const TriangleID closestId = QueryTriangleAt(localPosition, verticalDownRange, verticalUpRange, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, pFilter);
		if (closestId.IsValid())
		{
			if (pTriangleId)
				*pTriangleId = closestId;
			
			if (pSnappedLocalPosition)
			{
				MNM::vector3_t vertices[3];
				GetVertices(closestId, vertices);
				if (!Utils::ProjectPointOnTriangleVertical(localPosition, vertices[0], vertices[1], vertices[2], *pSnappedLocalPosition))
				{
					break;
				}
			}
			return true;
		}
		break;
	}
	case SSnappingMetric::EType::Box:
	case SSnappingMetric::EType::Circular:
	{
		const MNM::real_t horizontalRange = snappingMetric.horizontalRange == -FLT_MAX ? horizontalDefaultRange : MNM::real_t(snappingMetric.horizontalRange);
		const MNM::aabb_t localAabb(MNM::vector3_t(-horizontalRange, -horizontalRange, -verticalDownRange), MNM::vector3_t(horizontalRange, horizontalRange, verticalUpRange));
		const MNM::real_t maxDistance = snappingMetric.type == SSnappingMetric::EType::Circular ? horizontalRange : real_t::max();
		const MNM::SClosestTriangle closestTriangle = QueryClosestTriangle(localPosition, localAabb, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, maxDistance, pFilter);
		if (closestTriangle.id.IsValid())
		{
			if (pSnappedLocalPosition)
				*pSnappedLocalPosition = closestTriangle.position;
			if (pTriangleId)
				*pTriangleId = closestTriangle.id;
			return true;
		}
		break;
	}
	default:
		CRY_ASSERT_MESSAGE(false, "CNavMesh::SnapPosition: Unhandled snap metric type!");
		break;
	}
	return false;
}

bool CNavMesh::PushPointInsideTriangle(const TriangleID triangleID, vector3_t& localPosition, real_t amount) const
{
	if (amount <= 0)
		return false;

	vector3_t v0, v1, v2;
	if (const TileID tileID = ComputeTileID(triangleID))
	{
		const TileContainer& container = m_tiles[tileID - 1];
		const vector3_t tileOrigin = GetTileOrigin(container.x, container.y, container.z);
		const vector3_t positionInTile = localPosition - tileOrigin;

		const Tile::STriangle& triangle = container.tile.triangles[ComputeTriangleIndex(triangleID)];
		v0 = vector3_t(container.tile.vertices[triangle.vertex[0]]);
		v1 = vector3_t(container.tile.vertices[triangle.vertex[1]]);
		v2 = vector3_t(container.tile.vertices[triangle.vertex[2]]);

		const vector3_t triangleCenter = ((v0 + v1 + v2) * real_t::fraction(1, 3));
		const vector3_t locationToCenter = triangleCenter - positionInTile;
		const real_t locationToCenterLen = locationToCenter.lenNoOverflow();

		if (locationToCenterLen > amount)
		{
			localPosition += (locationToCenter / locationToCenterLen) * amount;
		}
		else
		{
			// If locationToCenterLen is smaller than the amount I wanna push
			// the point, then it's safer use directly the center position
			// otherwise the point could end up outside the other side of the triangle
			localPosition = triangleCenter + tileOrigin;
		}
		return true;
	}

	return false;
}

void CNavMesh::PredictNextTriangleEntryPosition(const TriangleID bestNodeTriangleID,
                                                const vector3_t& bestNodeLocalPosition, const TriangleID nextTriangleID, const unsigned int edgeIndex,
                                                const vector3_t& finalLocalPosition, vector3_t& outLocalPosition) const
{
	IF_UNLIKELY (edgeIndex == MNM::Constants::InvalidEdgeIndex)
	{
		// For SO links we don't set up the edgeIndex value since it's more probable that the animations
		// ending point is better approximated by the triangle center value
		vector3_t v0, v1, v2;
		GetVertices(nextTriangleID, v0, v1, v2);
		outLocalPosition = (v0 + v1 + v2) * real_t::fraction(1, 3);
		return;
	}

	Tile::STriangle triangle;
	if (GetTriangle(bestNodeTriangleID, triangle))
	{
		const TileID bestTriangleTileID = ComputeTileID(bestNodeTriangleID);
		assert(bestTriangleTileID);
		const TileContainer& currentContainer = m_tiles[bestTriangleTileID - 1];
		const STile& currentTile = currentContainer.tile;
		const vector3_t tileOrigin = GetTileOrigin(currentContainer.x, currentContainer.y, currentContainer.z);

		assert(edgeIndex < 3);
		const vector3_t v0 = tileOrigin + vector3_t(currentTile.vertices[triangle.vertex[edgeIndex]]);
		const vector3_t v1 = tileOrigin + vector3_t(currentTile.vertices[triangle.vertex[Utils::next_mod3(edgeIndex)]]);

		switch (gAIEnv.CVars.pathfinder.MNMPathfinderPositionInTrianglePredictionType)
		{
		case ePredictionType_TriangleCenter:
			{
				const vector3_t v2 = tileOrigin + vector3_t(currentTile.vertices[triangle.vertex[dec_mod3[edgeIndex]]]);
				outLocalPosition = (v0 + v1 + v2) * real_t::fraction(1, 3);
			}
			break;
		case ePredictionType_Advanced:
		default:
			{
				const vector3_t v0v1 = v1 - v0;
				real_t s, t;
				if (Utils::IntersectSegmentSegment(v0, v1, bestNodeLocalPosition, finalLocalPosition, s, t))
				{
					// If the two segments intersect,
					// let's choose the point that goes in the direction we want to go
					s = Utils::clamp(s, kMinPullingThreshold, kMaxPullingThreshold);
					outLocalPosition = v0 + v0v1 * s;
				}
				else
				{
					// Otherwise we need to understand where the segment is in relation
					// of where we want to go.
					// Let's choose the point on the segment that is closer towards the target
					const real_t::unsigned_overflow_type distSqAE = (v0 - finalLocalPosition).lenSqNoOverflow();
					const real_t::unsigned_overflow_type distSqBE = (v1 - finalLocalPosition).lenSqNoOverflow();
					const real_t segmentPercentage = (distSqAE < distSqBE) ? kMinPullingThreshold : kMaxPullingThreshold;
					outLocalPosition = v0 + v0v1 * segmentPercentage;
				}
			}
			break;
		}
	}
	else
	{
		// At this point it's not acceptable to have an invalid triangle
		assert(0);
	}
}

CNavMesh::EWayQueryResult CNavMesh::FindWay(SWayQueryRequest& inputRequest, SWayQueryWorkingSet& workingSet, SWayQueryResult& result) const
{
	if (inputRequest.GetFilter())
	{
		return FindWayInternal(inputRequest, workingSet, *inputRequest.GetFilter(), result);
	}
	else
	{
		return FindWayInternal(inputRequest, workingSet, DefaultQueryFilters::g_globalFilter, result);
	}
}

struct CostAccumulator
{
	CostAccumulator(const Vec3& locationToEval, const Vec3& startingLocation, real_t& totalCost)
		: m_totalCost(totalCost)
		, m_locationToEvaluate(locationToEval)
		, m_startingLocation(startingLocation)
	{}

	void operator()(const DangerAreaConstPtr& dangerInfo)
	{
		m_totalCost += dangerInfo->GetDangerHeuristicCost(m_locationToEvaluate, m_startingLocation);
	}
private:
	real_t & m_totalCost;
	const Vec3& m_locationToEvaluate;
	const Vec3& m_startingLocation;
};

real_t CalculateHeuristicCostForDangers(const vector3_t& locationToEval, const vector3_t& startingLocation, const Vec3& meshOrigin, const DangerousAreasList& dangersInfos)
{
	real_t totalCost(0.0f);
	const Vec3 startingLocationInWorldSpace = startingLocation.GetVec3() + meshOrigin;
	const Vec3 locationInWorldSpace = locationToEval.GetVec3() + meshOrigin;
	std::for_each(dangersInfos.begin(), dangersInfos.end(), CostAccumulator(locationInWorldSpace, startingLocationInWorldSpace, totalCost));
	return totalCost;
}

template<typename TFilter>
CNavMesh::EWayQueryResult CNavMesh::FindWayInternal(SWayQueryRequest& inputRequest, SWayQueryWorkingSet& workingSet, const TFilter& filter, SWayQueryResult& result) const
{
	result.SetWaySize(0);
	if (result.GetWayMaxSize() < 2)
	{
		return eWQR_Done;
	}

	if (inputRequest.From() && inputRequest.To())
	{
		if (inputRequest.From() == inputRequest.To())
		{
			WayTriangleData* pOutputWay = result.GetWayData();
			pOutputWay[0] = WayTriangleData(inputRequest.From(), OffMeshLinkID());
			pOutputWay[1] = WayTriangleData(inputRequest.To(), OffMeshLinkID());
			result.SetWaySize(2);
			return eWQR_Done;
		}
		else
		{
			const Vec3& meshOrigin = m_params.origin;
			const vector3_t mnmMeshOrigin = vector3_t(m_params.origin);
			const vector3_t startLocation = inputRequest.GetFromLocation();
			const vector3_t endLocation = inputRequest.GetToLocation();

			WayTriangleData lastBestNodeID(inputRequest.From(), OffMeshLinkID());

			while (workingSet.aStarNodesList.CanDoStep())
			{
				// switch the smallest element with the last one and pop the last element
				AStarNodesList::OpenNodeListElement element = workingSet.aStarNodesList.PopBestNode();
				WayTriangleData bestNodeID = element.triData;

				lastBestNodeID = bestNodeID;
				IF_UNLIKELY (bestNodeID.triangleID == inputRequest.To())
				{
					workingSet.aStarNodesList.StepDone();
					break;
				}

				AStarNodesList::Node* bestNode = element.pNode;
				const vector3_t bestNodeLocation = bestNode->location - mnmMeshOrigin;

				const TileID tileID = ComputeTileID(bestNodeID.triangleID);

				// Clear from previous step
				workingSet.nextLinkedTriangles.clear();

				if (tileID)
				{
					const TileContainer& container = m_tiles[tileID - 1];
					const STile& tile = container.tile;
					const uint16 triangleIdx = ComputeTriangleIndex(bestNodeID.triangleID);
					const Tile::STriangle& triangle = tile.triangles[triangleIdx];

					//Gather all accessible triangles first

					for (size_t l = 0; l < triangle.linkCount; ++l)
					{
						const Tile::SLink& link = tile.links[triangle.firstLink + l];

						WayTriangleData nextTri = WayTriangleData();

						if (link.side == Tile::SLink::Internal)
						{
							Tile::STriangle& neighbourTriangle = GetTriangleUnsafe(tileID, link.triangle);
							if (!filter.PassFilter(neighbourTriangle))
							{
								continue;
							}
							nextTri.triangleID = ComputeTriangleID(tileID, link.triangle);
							nextTri.incidentEdge = link.edge;
							nextTri.costMultiplier = filter.GetCostMultiplier(triangle);
							workingSet.nextLinkedTriangles.push_back(nextTri);
						}
						else if (link.side == Tile::SLink::OffMesh)
						{
							OffMeshNavigation::QueryLinksResult links = inputRequest.GetOffMeshNavigation().GetLinksForTriangle(bestNodeID.triangleID, link.triangle);
							while (nextTri = links.GetNextTriangle())
							{
								AreaAnnotation linkAnnotation;
								if (const IOffMeshLink* pLink = inputRequest.GetOffMeshLinkAndAnnotation(nextTri.offMeshLinkID, linkAnnotation))
								{
									if (filter.PassFilter(linkAnnotation))
									{
										nextTri.costMultiplier = filter.GetCostMultiplier(linkAnnotation);
										if (inputRequest.CanUseOffMeshLink(pLink, &nextTri.costMultiplier))
										{
											workingSet.nextLinkedTriangles.push_back(nextTri);
											nextTri.incidentEdge = (unsigned int)MNM::Constants::InvalidEdgeIndex;
										}
									}
								}
							}
							continue;
						}
						else
						{
							TileID neighbourTileID = GetNeighbourTileID(container.x, container.y, container.z, link.side);
							Tile::STriangle& neighbourTriangle = GetTriangleUnsafe(neighbourTileID, link.triangle);
							if (!filter.PassFilter(neighbourTriangle))
							{
								continue;
							}
							nextTri.triangleID = ComputeTriangleID(neighbourTileID, link.triangle);
							nextTri.incidentEdge = link.edge;
							nextTri.costMultiplier = filter.GetCostMultiplier(triangle);
							workingSet.nextLinkedTriangles.push_back(nextTri);
						}

						//////////////////////////////////////////////////////////////////////////
						// NOTE: This is user defined only at compile time

						m_tiles.BreakOnInvalidTriangle(nextTri.triangleID);

						//////////////////////////////////////////////////////////////////////////
					}
				}
				else
				{
					AIError("CNavMesh::FindWay - Bad Navmesh data Tile: %d, Triangle: %d, skipping ", tileID, ComputeTriangleIndex(bestNodeID.triangleID));
					m_tiles.BreakOnInvalidTriangle(bestNodeID.triangleID);
				}

				const size_t triangleCount = workingSet.nextLinkedTriangles.size();
				for (size_t t = 0; t < triangleCount; ++t)
				{
					WayTriangleData nextTri = workingSet.nextLinkedTriangles[t];

					if (nextTri == bestNode->prevTriangle)
						continue;

					AStarNodesList::Node* nextNode = NULL;
					const bool inserted = workingSet.aStarNodesList.InsertNode(nextTri, &nextNode);

					assert(nextNode);
					vector3_t nextNodeLocation;
					if (inserted)
					{
						IF_UNLIKELY (nextTri.triangleID == inputRequest.To())
						{
							nextNodeLocation = endLocation;
						}
						else
						{
							PredictNextTriangleEntryPosition(bestNodeID.triangleID, bestNodeLocation, nextTri.triangleID, nextTri.incidentEdge, endLocation, nextNodeLocation);
						}

						nextNode->open = false;
					}
					else
					{
						nextNodeLocation = nextNode->location - mnmMeshOrigin;
					}

					//Euclidean distance
					const vector3_t targetDistance = endLocation - nextNodeLocation;
					const vector3_t stepDistance = bestNodeLocation - nextNodeLocation;
					const real_t heuristic = targetDistance.lenNoOverflow();
					const real_t stepCost = stepDistance.lenNoOverflow();

					//Euclidean distance approximation
					//const real_t heuristic = targetDistance.approximatedLen();
					//const real_t stepCost = stepDistance.approximatedLen();

					const real_t dangersTotalCost = CalculateHeuristicCostForDangers(nextNodeLocation, startLocation, meshOrigin, inputRequest.GetDangersInfos());
					const real_t customCost = CalculateHeuristicCostForCustomRules(bestNode->location, nextNodeLocation, meshOrigin, inputRequest.GetCustomPathCostComputer().get());

					real_t costMultiplier = real_t(nextTri.costMultiplier);

					const real_t cost = bestNode->cost + (stepCost * costMultiplier) + dangersTotalCost + customCost;
					const real_t total = cost + heuristic;

					if (nextNode->open && nextNode->estimatedTotalCost <= total)
						continue;

					nextNode->cost = cost;
					nextNode->estimatedTotalCost = total;
					nextNode->prevTriangle = bestNodeID;

					if (!nextNode->open)
					{
						nextNode->open = true;
						nextNode->location = nextNodeLocation + mnmMeshOrigin;
						workingSet.aStarNodesList.AddToOpenList(nextTri, nextNode, total);
					}
				}

				workingSet.aStarNodesList.StepDone();
			}

			if (lastBestNodeID.triangleID == inputRequest.To())
			{
				size_t wayTriCount = 0;
				WayTriangleData wayTriangle = lastBestNodeID;
				WayTriangleData nextInsertion(wayTriangle.triangleID, OffMeshLinkID());

				WayTriangleData* outputWay = result.GetWayData();

				while (wayTriangle.triangleID != inputRequest.From())
				{
					const AStarNodesList::Node* node = workingSet.aStarNodesList.FindNode(wayTriangle);
					assert(node);
					outputWay[wayTriCount++] = nextInsertion;

					//Iterate the path backwards, and shift the offMeshLink to the previous triangle (start of the link)
					nextInsertion.offMeshLinkID = wayTriangle.offMeshLinkID;
					wayTriangle = node->prevTriangle;
					nextInsertion.triangleID = wayTriangle.triangleID;

					if (wayTriCount == result.GetWayMaxSize())
						break;
				}

				if (wayTriCount < result.GetWayMaxSize())
				{
					outputWay[wayTriCount++] = WayTriangleData(inputRequest.From(), nextInsertion.offMeshLinkID);
				}

				result.SetWaySize(wayTriCount);
				return eWQR_Done;
			}
			else if (!workingSet.aStarNodesList.Empty())
			{
				//We did not finish yet...
				return eWQR_Continuing;
			}
		}
	}

	return eWQR_Done;
}

real_t CNavMesh::CalculateHeuristicCostForCustomRules(const vector3_t& locationComingFrom, const vector3_t& locationGoingTo, const Vec3& meshOrigin, const IMNMCustomPathCostComputer* pCustomPathCostComputer) const
{
	if (pCustomPathCostComputer)
	{
		static const IMNMCustomPathCostComputer::ComputationFlags flags{ IMNMCustomPathCostComputer::EComputationType::Cost };

		const Vec3 locationComingFromInWorldSpace = locationComingFrom.GetVec3() + meshOrigin;
		const Vec3 locationGoingToInWorldSpace = locationGoingTo.GetVec3() + meshOrigin;
		const IMNMCustomPathCostComputer::SComputationInput computationInput(flags, locationComingFromInWorldSpace, locationGoingToInWorldSpace);
		IMNMCustomPathCostComputer::SComputationOutput computationOutput;
		pCustomPathCostComputer->ComputeCostThreadUnsafe(computationInput, computationOutput);
		return computationOutput.cost;
	}
	else
	{
		return real_t(0);
	}
}

void CNavMesh::PullString(const vector3_t& fromLocalPosition, const TriangleID fromTriID, const vector3_t& toLocalPosition, const TriangleID toTriID, vector3_t& middleLocalPosition) const
{
	if (const TileID fromTileID = ComputeTileID(fromTriID))
	{
		const TileContainer& startContainer = m_tiles[fromTileID - 1];
		const STile& startTile = startContainer.tile;
		uint16 fromTriangleIdx = ComputeTriangleIndex(fromTriID);
		const Tile::STriangle& fromTriangle = startTile.triangles[fromTriangleIdx];

		uint16 vi0 = 0, vi1 = 0;
		for (int l = 0; l < fromTriangle.linkCount; ++l)
		{
			const Tile::SLink& link = startTile.links[fromTriangle.firstLink + l];
			if (link.side == Tile::SLink::Internal)
			{
				TriangleID newTriangleID = ComputeTriangleID(fromTileID, link.triangle);
				if (newTriangleID == toTriID)
				{
					vi0 = link.edge;
					vi1 = Utils::next_mod3(link.edge);
					assert(vi0 < 3);
					assert(vi1 < 3);
					break;
				}
			}
			else
			{
				if (link.side != Tile::SLink::OffMesh)
				{
					TileID neighbourTileID = GetNeighbourTileID(startContainer.x, startContainer.y, startContainer.z, link.side);
					TriangleID newTriangleID = ComputeTriangleID(neighbourTileID, link.triangle);

					if (newTriangleID == toTriID)
					{
						vi0 = link.edge;
						vi1 = Utils::next_mod3(link.edge);
						assert(vi0 < 3);
						assert(vi1 < 3);
						break;
					}
				}
			}
		}

		vector3_t fromVertices[3];
		GetVertices(fromTriID, fromVertices[0], fromVertices[1], fromVertices[2]);

		if (vi0 != vi1)
		{
			PREFAST_ASSUME(vi0 < 3 && vi1 < 3);

			real_t s, t;
			if (!Utils::IntersectSegmentSegment(vector2_t(fromVertices[vi0]),
			                             vector2_t(fromVertices[vi1]), vector2_t(fromLocalPosition), vector2_t(toLocalPosition), s, t))
			{
				s = 0;
			}

			// Even if segments don't intersect, s = 0 and is clamped to the pulling threshold range
			s = Utils::clamp(s, kMinPullingThreshold, kMaxPullingThreshold);

			middleLocalPosition = Lerp(fromVertices[vi0], fromVertices[vi1], s);
		}
	}
}

void CNavMesh::MarkTrianglesNotConnectedToSeeds(const MNM::AreaAnnotation::value_type markFlags)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	for (TileMap::const_iterator it = m_tileMap.cbegin(); it != m_tileMap.cend(); ++it)
	{
		const TileID tileID = it->second;
		const STile& tile = m_tiles[tileID - 1].tile;		
		const uint16 triangleCount = tile.GetTrianglesCount();
		
		for (uint16 triIdx = 0; triIdx < triangleCount; ++triIdx)
		{
			Tile::STriangle& triangle = tile.triangles[triIdx];
			if(triangle.islandID == MNM::Constants::eStaticIsland_InvalidIslandID)
				continue;
			
			AreaAnnotation::value_type flags = triangle.areaAnnotation.GetFlags();
			if (m_islands.GetSeedConnectivityState(triangle.islandID) == CIslands::ESeedConnectivityState::Inaccessible)
			{
				flags |= markFlags;
			}
			else
			{
				flags &= ~markFlags;
			}
			triangle.areaAnnotation.SetFlags(flags);
		}
	}
}

void CNavMesh::RemoveTrianglesByFlags(const MNM::AreaAnnotation::value_type flags, const TrianglesSetsByTile& trianglesToUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	Tile::STriangle newTriangles[MNM::Constants::TileTrianglesMaxCount];
	Tile::Vertex newVertices[MNM::Constants::TileTrianglesMaxCount * 3];

	// This array is used for two things:
	// 1. Storing if the vertex is part of any triangle
	// 2. Storing vertex indices offsets for remapping
	uint16 verticesAuxArray[MNM::Constants::TileTrianglesMaxCount * 3];

	// Array used for remapping triangle ids in trianglesToUpdate
	uint16 trianglesAuxArray[MNM::Constants::TileTrianglesMaxCount];

	for (TileMap::const_iterator tileIt = m_tileMap.cbegin(); tileIt != m_tileMap.cend(); ++tileIt)
	{
		const TileID tileId = tileIt->second;
		STile& tile = m_tiles[tileId - 1].tile;

		CRY_ASSERT_MESSAGE(tile.nodeCount == 0, "Removing triangles from tiles with BV Tree isn't supported");
		CRY_ASSERT(tile.vertexCount <= MNM::Constants::TileTrianglesMaxCount * 3);
		memset(verticesAuxArray, 0, sizeof(verticesAuxArray[0]) * tile.vertexCount);

		// First copy all needed triangles and store whether the vertex is used in verticesAuxArray
		uint16 newTrianglesCount = 0;
		for (uint16 triangleIdx = 0; triangleIdx < tile.triangleCount; ++triangleIdx)
		{
			const Tile::STriangle& triangle = tile.triangles[triangleIdx];

			if ((triangle.areaAnnotation.GetFlags() & flags) != 0)
			{
				trianglesAuxArray[triangleIdx] = uint16(-1);
				continue;
			}

			trianglesAuxArray[triangleIdx] = newTrianglesCount;

			newTriangles[newTrianglesCount++] = triangle;
			for (uint16 v = 0; v < 3; ++v)
			{
				verticesAuxArray[triangle.vertex[v]] = 1;
			}
		}

		if (newTrianglesCount == tile.triangleCount)
			continue; // No triangles were removed

		// Update TriangleIds
		const auto it = trianglesToUpdate.find(tileId);
		if (it != trianglesToUpdate.end())
		{
			for (auto trianglesIt = it->second.cbegin(); trianglesIt != it->second.cend(); ++trianglesIt)
			{
				std::vector<TriangleID>& triangleIds = **trianglesIt;
				for (TriangleID& triangleId : triangleIds)
				{
					const TileID triangleTileId = ComputeTileID(triangleId);
					if (triangleTileId == tileId)
					{
						// Update only triangle ids from current tile
						const uint16 triangleIdx = ComputeTriangleIndex(triangleId);
						const uint16 newTriangleIdx = trianglesAuxArray[triangleIdx];
						triangleId = newTriangleIdx == uint16(-1) ? TriangleID() : ComputeTriangleID(tileId, newTriangleIdx);
					}
				}
			}
		}

		// Copy used vertices and compute offsets of vertex indices
		uint16 newVerticesCount = 0;
		uint16 currentOffset = 0;
		for (uint16 vertexIdx = 0; vertexIdx < tile.vertexCount; ++vertexIdx)
		{
			const bool isUsed = verticesAuxArray[vertexIdx] != 0;
			if (isUsed)
			{
				newVertices[newVerticesCount++] = tile.vertices[vertexIdx];
			}
			else
			{
				++currentOffset;
			}
			verticesAuxArray[vertexIdx] = currentOffset;
		}

		// Offset indices of vertices
		for (uint16 triangleIdx = 0; triangleIdx < newTrianglesCount; ++triangleIdx)
		{
			Tile::STriangle& triangle = newTriangles[triangleIdx];
			for (uint16 triVertexIdx = 0; triVertexIdx < 3; ++triVertexIdx)
			{
				triangle.vertex[triVertexIdx] -= verticesAuxArray[triangle.vertex[triVertexIdx]];
			}
		}

		tile.CopyTriangles(newTriangles, newTrianglesCount);
		tile.CopyVertices(newVertices, newVerticesCount);

		// Links aren't valid anymore after re-indexing triangles and need to be rebuilt
		ConnectToNetwork(tileId, nullptr);
	}
}

void CNavMesh::AddOffMeshLinkToTile(const TileID tileID, const TriangleID triangleID, const uint16 offMeshIndex)
{
	STile& tile = GetTile(tileID);

	m_profiler.FreeMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));
	m_profiler.AddStat(LinkCount, -(int)tile.linkCount);

	tile.AddOffMeshLink(triangleID, offMeshIndex);

	m_profiler.AddMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));
	m_profiler.AddStat(LinkCount, tile.linkCount);
}

void CNavMesh::UpdateOffMeshLinkForTile(const TileID tileID, const TriangleID triangleID, const uint16 offMeshIndex)
{
	STile& tile = GetTile(tileID);

	tile.UpdateOffMeshLink(triangleID, offMeshIndex);
}

void CNavMesh::RemoveOffMeshLinkFromTile(const TileID tileID, const TriangleID triangleID)
{
	STile& tile = GetTile(tileID);

	m_profiler.FreeMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));
	m_profiler.AddStat(LinkCount, -(int)tile.linkCount);

	tile.RemoveOffMeshLink(triangleID);

	m_profiler.AddMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));
	m_profiler.AddStat(LinkCount, tile.linkCount);
}

MNM::ERayCastResult CNavMesh::RayCast(const vector3_t& fromLocalPosition, TriangleID fromTri, const vector3_t& toLocalPosition, TriangleID toTri,
                                           RaycastRequestBase& raycastRequest, const INavMeshQueryFilter* pFilter) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	switch (gAIEnv.CVars.navigation.MNMRaycastImplementation)
	{
	case 0:
		return RayCast_v1(fromLocalPosition, fromTri, toLocalPosition, toTri, raycastRequest);
	case 1:
		return RayCast_v2(fromLocalPosition, fromTri, toLocalPosition, toTri, raycastRequest);
	case 2:
	default:
	{
		if (pFilter)
		{
			return RayCast_v3(fromLocalPosition, fromTri, toLocalPosition, toTri, *pFilter, raycastRequest);
		}
		else
		{
			return RayCast_v3(fromLocalPosition, fromTri, toLocalPosition, toTri, DefaultQueryFilters::g_globalFilter, raycastRequest);
		}
	}
	}
}

struct RaycastNode
{
	RaycastNode()
		: triangleID()
		, percentageOfTotalDistance(-1.0f)
		, incidentEdge((uint16) MNM::Constants::InvalidEdgeIndex)
	{}

	RaycastNode(const TriangleID _triangleID, const real_t _percentageOfTotalDistance, const uint16 _incidentEdge)
		: triangleID(_triangleID)
		, percentageOfTotalDistance(_percentageOfTotalDistance)
		, incidentEdge(_incidentEdge)
	{}

	RaycastNode(const RaycastNode& otherNode)
		: triangleID(otherNode.triangleID)
		, percentageOfTotalDistance(otherNode.percentageOfTotalDistance)
		, incidentEdge(otherNode.incidentEdge)
	{}

	ILINE bool IsValid() const { return triangleID.IsValid(); }

	ILINE bool operator<(const RaycastNode& other) const
	{
		return triangleID < other.triangleID;
	}

	ILINE bool operator==(const RaycastNode& other) const
	{
		return triangleID == other.triangleID;
	}

	TriangleID triangleID;
	real_t     percentageOfTotalDistance;
	uint16     incidentEdge;
};

struct IsNodeCloserToEndPredicate
{
	bool operator()(const RaycastNode& firstNode, const RaycastNode& secondNode) { return firstNode.percentageOfTotalDistance > secondNode.percentageOfTotalDistance; }
};

typedef OpenList<RaycastNode, IsNodeCloserToEndPredicate> RaycastOpenList;
typedef VectorSet<TriangleID>                             RaycastClosedList;

MNM::ERayCastResult CNavMesh::RayCast_v2(const vector3_t& fromLocalPosition, TriangleID fromTriangleID, const vector3_t& toLocalPosition, TriangleID toTriangleID,
	RaycastRequestBase& raycastRequest) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!IsLocationInTriangle(fromLocalPosition, fromTriangleID))
		return ERayCastResult::InvalidStart;

#ifdef RAYCAST_DO_NOT_ACCEPT_INVALID_END
	if (!IsLocationInTriangle(toLocalPosition, toTriangleID))
		return ERayCastResult::InvalidEnd;
#endif

	RaycastClosedList closedList;
	closedList.reserve(raycastRequest.maxWayTriCount);

	RaycastCameFromMap cameFrom;
	cameFrom.reserve(raycastRequest.maxWayTriCount);

	const size_t expectedOpenListSize = 10;
	RaycastOpenList openList(expectedOpenListSize);

	RaycastNode furtherNodeVisited;
	RayHit rayHit;

	RaycastNode currentNode(fromTriangleID, real_t(.0f), (uint16)MNM::Constants::InvalidEdgeIndex);

	while (currentNode.IsValid())
	{
		if (currentNode.triangleID == toTriangleID)
		{
			return ConstructRaycastResult(ERayCastResult::NoHit, rayHit, toTriangleID, cameFrom, raycastRequest);
		}

		if (currentNode.percentageOfTotalDistance > furtherNodeVisited.percentageOfTotalDistance)
			furtherNodeVisited = currentNode;

		rayHit.triangleID = currentNode.triangleID;
		RaycastNode nextNode;

		closedList.insert(currentNode.triangleID);

		TileID tileID = ComputeTileID(currentNode.triangleID);
		if (tileID.IsValid())
		{
			const TileContainer* container = &m_tiles[tileID - 1];
			const STile* tile = &container->tile;

			const vector3_t tileOrigin = GetTileOrigin(container->x, container->y, container->z);

			const Tile::STriangle& triangle = tile->triangles[ComputeTriangleIndex(currentNode.triangleID)];
			for (uint16 edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
			{
				if (currentNode.incidentEdge == edgeIndex)
					continue;

				const vector3_t a = tileOrigin + vector3_t(tile->vertices[triangle.vertex[edgeIndex]]);
				const vector3_t b = tileOrigin + vector3_t(tile->vertices[triangle.vertex[inc_mod3[edgeIndex]]]);

				real_t s, t;
				if (Utils::IntersectSegmentSegment(vector2_t(fromLocalPosition), vector2_t(toLocalPosition), vector2_t(a), vector2_t(b), s, t))
				{
					if (s < currentNode.percentageOfTotalDistance)
						continue;

					rayHit.distance = s;
					rayHit.edge = edgeIndex;

					for (size_t linkIndex = 0; linkIndex < triangle.linkCount; ++linkIndex)
					{
						const Tile::SLink& link = tile->links[triangle.firstLink + linkIndex];
						if (link.edge != edgeIndex)
							continue;

						const uint16 side = link.side;

						if (side == Tile::SLink::Internal)
						{
							const Tile::STriangle& opposite = tile->triangles[link.triangle];

							for (size_t oe = 0; oe < opposite.linkCount; ++oe)
							{
								const Tile::SLink& reciprocal = tile->links[opposite.firstLink + oe];
								const TriangleID possibleNextID = ComputeTriangleID(tileID, link.triangle);

								if (reciprocal.triangle == ComputeTriangleIndex(currentNode.triangleID))
								{
									if (closedList.find(possibleNextID) != closedList.end())
										break;

									cameFrom[possibleNextID] = currentNode.triangleID;

									if (s > currentNode.percentageOfTotalDistance)
									{
										nextNode.incidentEdge = reciprocal.edge;
										nextNode.percentageOfTotalDistance = s;
										nextNode.triangleID = possibleNextID;
									}
									else
									{
										RaycastNode possibleNextEdge(possibleNextID, s, reciprocal.edge);
										openList.InsertElement(possibleNextEdge);
									}

									break;
								}
							}
						}
						else if (side != Tile::SLink::OffMesh)
						{
							TileID neighbourTileID = GetNeighbourTileID(container->x, container->y, container->z, link.side);
							const TileContainer& neighbourContainer = m_tiles[neighbourTileID - 1];

							const Tile::STriangle& opposite = neighbourContainer.tile.triangles[link.triangle];

							const uint16 currentTriangleIndex = ComputeTriangleIndex(currentNode.triangleID);
							const uint16 currentOppositeSide = NavMesh::GetOppositeSide(side);

							for (size_t reciprocalLinkIndex = 0; reciprocalLinkIndex < opposite.linkCount; ++reciprocalLinkIndex)
							{
								const Tile::SLink& reciprocal = neighbourContainer.tile.links[opposite.firstLink + reciprocalLinkIndex];
								if ((reciprocal.triangle == currentTriangleIndex) && (reciprocal.side == currentOppositeSide))
								{
									const TriangleID possibleNextID = ComputeTriangleID(neighbourTileID, link.triangle);

									if (closedList.find(possibleNextID) != closedList.end())
										break;

									const vector3_t neighbourTileOrigin = GetTileOrigin(neighbourContainer.x, neighbourContainer.y, neighbourContainer.z);

									const uint16 i0 = reciprocal.edge;
									const uint16 i1 = inc_mod3[reciprocal.edge];

									assert(i0 < 3);
									assert(i1 < 3);

									const vector3_t c = neighbourTileOrigin + vector3_t(neighbourContainer.tile.vertices[opposite.vertex[i0]]);
									const vector3_t d = neighbourTileOrigin + vector3_t(neighbourContainer.tile.vertices[opposite.vertex[i1]]);

									real_t p, q;
									if (Utils::IntersectSegmentSegment(vector2_t(fromLocalPosition), vector2_t(toLocalPosition), vector2_t(c), vector2_t(d), p, q))
									{
										cameFrom[possibleNextID] = currentNode.triangleID;

										if (p > currentNode.percentageOfTotalDistance)
										{
											nextNode.incidentEdge = reciprocal.edge;
											nextNode.percentageOfTotalDistance = p;
											nextNode.triangleID = possibleNextID;
										}
										else
										{
											RaycastNode possibleNextEdge(possibleNextID, p, reciprocal.edge);
											openList.InsertElement(possibleNextEdge);
										}

										break;   // reciprocal link loop
									}
								}
							}
						}

						if (nextNode.IsValid())
							break;
					}

					if (nextNode.IsValid())
						break;

				}
			}
		}

		if (nextNode.IsValid())
		{
			openList.Reset();
			currentNode = nextNode;
		}
		else
		{
			currentNode = (!openList.IsEmpty()) ? openList.PopBestElement() : RaycastNode();
		}

	}

	return ConstructRaycastResult(ERayCastResult::Hit, rayHit, furtherNodeVisited.triangleID, cameFrom, raycastRequest);
}

template<typename TFilter>
MNM::ERayCastResult CNavMesh::RayCast_v3(const vector3_t& fromLocalPosition, TriangleID fromTriangleID, const vector3_t& toLocalPosition, TriangleID toTriangleID, const TFilter& filter, RaycastRequestBase& raycastRequest) const
{
	//TODO: take area costs into account and return total cost to traverse the ray
	
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!IsLocationInTriangle(fromLocalPosition, fromTriangleID))
		return ERayCastResult::InvalidStart;

	RaycastCameFromMap cameFrom;
	cameFrom.reserve(raycastRequest.maxWayTriCount);

	RayHit rayHit;
	rayHit.distance = 0.0f;
	rayHit.edge = MNM::Constants::InvalidEdgeIndex;
	rayHit.triangleID = TriangleID();

	TriangleID currentTriangleID = fromTriangleID;
	
	while (currentTriangleID.IsValid())
	{
		const TileID currentTileID = ComputeTileID(currentTriangleID);
		CRY_ASSERT(currentTileID.IsValid());

		if (!currentTileID.IsValid())
			break;
		
		const TileContainer& currentContainer = m_tiles[currentTileID - 1];
		const STile& currentTile = currentContainer.tile;

		const uint16 currentTriangleIndex = ComputeTriangleIndex(currentTriangleID);
		const Tile::STriangle& currentTriangle = currentTile.triangles[currentTriangleIndex];

		const vector3_t currentTileOrigin = GetTileOrigin(currentContainer.x, currentContainer.y, currentContainer.z);

		//collect vertices of the current triangle
		vector2_t currentTriangleVertices[3];
		currentTriangleVertices[0] = vector2_t(currentTileOrigin + vector3_t(currentContainer.tile.vertices[currentTriangle.vertex[0]]));
		currentTriangleVertices[1] = vector2_t(currentTileOrigin + vector3_t(currentContainer.tile.vertices[currentTriangle.vertex[1]]));
		currentTriangleVertices[2] = vector2_t(currentTileOrigin + vector3_t(currentContainer.tile.vertices[currentTriangle.vertex[2]]));
		
		real_t rayIntersectionParam;
		uint16 intersectionEdgeIndex;
		const bool bEndingInside = FindNextIntersectingTriangleEdge(fromLocalPosition, toLocalPosition, currentTriangleVertices, rayIntersectionParam, intersectionEdgeIndex);
		if (rayIntersectionParam >= real_t(1.0f))
		{
			if (!bEndingInside && intersectionEdgeIndex == uint16(MNM::Constants::InvalidEdgeIndex))
			{
				// Ray segment missed the triangle, return the last hit
				return ConstructRaycastResult(ERayCastResult::Hit, rayHit, rayHit.triangleID, cameFrom, raycastRequest);
			}
			
			rayHit.triangleID = currentTriangleID;
			rayHit.edge = intersectionEdgeIndex;
			rayHit.distance = 1.0f;
			if (currentTriangleID == toTriangleID || !toTriangleID.IsValid())
			{
				// Ray segment is ending in end triangle or position, return no hit
				return ConstructRaycastResult(ERayCastResult::NoHit, rayHit, rayHit.triangleID, cameFrom, raycastRequest);
			}
			else
			{
				if (intersectionEdgeIndex != uint16(MNM::Constants::InvalidEdgeIndex) && IsPointInsideNeighbouringTriangleWhichSharesEdge(currentTriangleID, intersectionEdgeIndex, toLocalPosition, toTriangleID))
				{
					// Ray segment is ending in end triangle, return no hit
					// TODO: construct full sequence of triangles to toTriangleID? 
					return ConstructRaycastResult(ERayCastResult::NoHit, rayHit, rayHit.triangleID, cameFrom, raycastRequest);
				}
				else
				{
					// Ray segment not is ending in end triangle, end triangle is probably in different layer
					return ConstructRaycastResult(ERayCastResult::DisconnectedLocations, rayHit, rayHit.triangleID, cameFrom, raycastRequest);
				}
			}
		}

		rayHit.triangleID = currentTriangleID;
		rayHit.edge = intersectionEdgeIndex;
		if (rayIntersectionParam > rayHit.distance)
		{
			rayHit.distance = rayIntersectionParam;
		}
		
		TriangleID neighbourTriangleID = StepOverEdgeToNeighbourTriangle(fromLocalPosition, toLocalPosition, currentTileID, currentTriangleID, intersectionEdgeIndex, filter);
		if (neighbourTriangleID.IsValid())
		{
			std::pair<RaycastCameFromMap::iterator, bool> insertResult = cameFrom.insert({ neighbourTriangleID, currentTriangleID });
			if (!insertResult.second)
			{
				// Triangle was already visited, we have a loop
				// This shouldn't happen in normal circumstances and it can mean that we have e.g. degenerate triangle
				neighbourTriangleID = TriangleID();
			}
		}
		currentTriangleID = neighbourTriangleID;
	}

	return ConstructRaycastResult(ERayCastResult::Hit, rayHit, rayHit.triangleID, cameFrom, raycastRequest);
}

template<typename TFilter>
TriangleID CNavMesh::StepOverEdgeToNeighbourTriangle(const vector3_t& rayStart, const vector3_t& rayEnd, const TileID currentTileID, const TriangleID currentTriangleID, const uint16 edgeIndex, const TFilter& filter) const
{
	const TileContainer& currentContainer = m_tiles[currentTileID - 1];
	const STile& currentTile = currentContainer.tile;

	const uint16 currentTriangleIndex = ComputeTriangleIndex(currentTriangleID);
	const Tile::STriangle& currentTriangle = currentTile.triangles[currentTriangleIndex];

	for (size_t linkIndex = 0; linkIndex < currentTriangle.linkCount; ++linkIndex)
	{
		// Find link to neighbour triangle corresponding to found edge
		const Tile::SLink& link = currentTile.links[currentTriangle.firstLink + linkIndex];
		if (link.edge != edgeIndex)
			continue;

		const uint16 side = link.side;

		if(side == Tile::SLink::OffMesh)
			// Not interested in offmesh links
			continue;

		if (side == Tile::SLink::Internal)
		{
			// Internal link between two triangles in the same tile
			const Tile::STriangle& neighbourTriangle = GetTriangleUnsafe(currentTileID, link.triangle);
			return filter.PassFilter(neighbourTriangle) ? ComputeTriangleID(currentTileID, link.triangle) : TriangleID();
		}

		// Edge is on the tile boundaries, there can be more neighbour triangles adjacent to this edge
		const TileID neighbourTileID = GetNeighbourTileID(currentContainer.x, currentContainer.y, currentContainer.z, link.side);
		const TileContainer& neighbourContainer = m_tiles[neighbourTileID - 1];
		const Tile::STriangle& neighbourTriangle = neighbourContainer.tile.triangles[link.triangle];
		
		if(!filter.PassFilter(neighbourTriangle))
			continue;

		const uint16 currentOppositeSide = NavMesh::GetOppositeSide(side);

		for (size_t reciprocalLinkIndex = 0; reciprocalLinkIndex < neighbourTriangle.linkCount; ++reciprocalLinkIndex)
		{
			const Tile::SLink& reciprocal = neighbourContainer.tile.links[neighbourTriangle.firstLink + reciprocalLinkIndex];
			if ((reciprocal.triangle == currentTriangleIndex) && (reciprocal.side == currentOppositeSide))
			{
				const vector3_t neighbourTileOrigin = GetTileOrigin(neighbourContainer.x, neighbourContainer.y, neighbourContainer.z);

				const uint16 i0 = reciprocal.edge;
				const uint16 i1 = inc_mod3[reciprocal.edge];

				CRY_ASSERT(i0 < 3);
				CRY_ASSERT(i1 < 3);

				const vector3_t edgeStart = neighbourTileOrigin + vector3_t(neighbourContainer.tile.vertices[neighbourTriangle.vertex[i0]]);
				const vector3_t edgeEnd = neighbourTileOrigin + vector3_t(neighbourContainer.tile.vertices[neighbourTriangle.vertex[i1]]);

				// TODO: This could be optimized by using variation of IntersectSegmentSegment function, 
				// which doesn't compute intersection parameters since we are not interested in them in this case
				// Moreover we probably only need to check whether the intersection is lying on the edge (ray segment was checked in previous parts of the code).
				real_t rayParameter, edgeParameter;
				if (MNM::Utils::IntersectSegmentSegment(vector2_t(rayStart), vector2_t(rayEnd), vector2_t(edgeStart), vector2_t(edgeEnd), rayParameter, edgeParameter))
				{
					return ComputeTriangleID(neighbourTileID, link.triangle);
				}
			}
		}
	}

	return TriangleID();
}

MNM::ERayCastResult CNavMesh::ConstructRaycastResult(const ERayCastResult returnResult, const RayHit& rayHit, const TriangleID lastTriangleID, const RaycastCameFromMap& cameFromMap,
	RaycastRequestBase& raycastRequest) const
{
	raycastRequest.hit = rayHit;
	
	TriangleID currentTriangleID = lastTriangleID;
	size_t elementIndex = 0;
	RaycastCameFromMap::const_iterator elementIterator = cameFromMap.find(currentTriangleID);
	while (elementIterator != cameFromMap.end())
	{
		if (elementIndex >= raycastRequest.maxWayTriCount)
		{
			raycastRequest.result = ERayCastResult::RayTooLong;
			return ERayCastResult::RayTooLong;
		}

		raycastRequest.way[elementIndex++] = currentTriangleID;
		currentTriangleID = elementIterator->second;
		elementIterator = cameFromMap.find(currentTriangleID);
	}

	raycastRequest.way[elementIndex++] = currentTriangleID;
	raycastRequest.wayTriCount = elementIndex;
	raycastRequest.result = elementIndex < raycastRequest.maxWayTriCount ? returnResult : ERayCastResult::RayTooLong;

	return returnResult;
}

bool CNavMesh::IsLocationInTriangle(const vector3_t& localPosition, const TriangleID triangleID) const
{
	if (!triangleID.IsValid())
		return false;

	TileID tileID = ComputeTileID(triangleID);
	if (!tileID.IsValid())
		return false;

	const TileContainer* container = &m_tiles[tileID - 1];
	const STile* tile = &container->tile;

	const vector3_t tileOrigin = GetTileOrigin(container->x, container->y, container->z);

	if (triangleID)
	{
		const Tile::STriangle& triangle = tile->triangles[ComputeTriangleIndex(triangleID)];

		const vector2_t a = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[0]]);
		const vector2_t b = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[1]]);
		const vector2_t c = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[2]]);

		return Utils::PointInTriangle(vector2_t(localPosition), a, b, c);
	}

	return false;
}

MNM::ERayCastResult CNavMesh::RayCast_v1(const vector3_t& fromLocalPosition, TriangleID fromTri, const vector3_t& toLocalPosition, TriangleID toTri, RaycastRequestBase& raycastRequest) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (TileID tileID = ComputeTileID(fromTri))
	{
		const TileContainer* container = &m_tiles[tileID - 1];
		const STile* tile = &container->tile;

		vector3_t tileOrigin = GetTileOrigin(container->x, container->y, container->z);

		if (fromTri)
		{
			const Tile::STriangle& triangle = tile->triangles[ComputeTriangleIndex(fromTri)];

			const vector2_t a = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[0]]);
			const vector2_t b = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[1]]);
			const vector2_t c = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[2]]);

			if (!Utils::PointInTriangle(vector2_t(fromLocalPosition), a, b, c))
				fromTri = TriangleID();
		}

		if (!fromTri)
		{
			RayHit& hit = raycastRequest.hit;
			hit.distance = -real_t::max();
			hit.triangleID = TriangleID();
			hit.edge = 0;

			return ERayCastResult::Hit;
		}

		real_t distance = -1;
		size_t triCount = 0;
		TriangleID currentID = fromTri;
		size_t incidentEdge = MNM::Constants::InvalidEdgeIndex;

		while (currentID)
		{
			if (triCount < raycastRequest.maxWayTriCount)
			{
				raycastRequest.way[triCount++] = currentID;
			}
			else
			{
				// We don't allow rays that pass through more than maxWayTriCount triangles
				return ERayCastResult::RayTooLong;
			}

			if (toTri && currentID == toTri)
			{
				raycastRequest.wayTriCount = triCount;
				return ERayCastResult::NoHit;
			}

			const Tile::STriangle& triangle = tile->triangles[ComputeTriangleIndex(currentID)];
			TriangleID nextID = TriangleID();
			real_t possibleDistance = distance;
			size_t possibleIncidentEdge = MNM::Constants::InvalidEdgeIndex;
			const TileContainer* possibleContainer = NULL;
			const STile* possibleTile = NULL;
			vector3_t possibleTileOrigin(tileOrigin);
			TileID possibleTileID = tileID;

			for (size_t e = 0; e < 3; ++e)
			{
				if (incidentEdge == e)
					continue;

				const vector3_t a = tileOrigin + vector3_t(tile->vertices[triangle.vertex[e]]);
				const vector3_t b = tileOrigin + vector3_t(tile->vertices[triangle.vertex[Utils::next_mod3(e)]]);

				real_t s, t;
				if (Utils::IntersectSegmentSegment(vector2_t(fromLocalPosition), vector2_t(toLocalPosition), vector2_t(a), vector2_t(b), s, t))
				{
					if (s < distance)
						continue;

					for (size_t l = 0; l < triangle.linkCount; ++l)
					{
						const Tile::SLink& link = tile->links[triangle.firstLink + l];
						if (link.edge != e)
							continue;

						const size_t side = link.side;

						if (side == Tile::SLink::Internal)
						{
							const Tile::STriangle& opposite = tile->triangles[link.triangle];

							for (size_t oe = 0; oe < opposite.linkCount; ++oe)
							{
								const Tile::SLink& reciprocal = tile->links[opposite.firstLink + oe];
								const TriangleID possibleNextID = ComputeTriangleID(tileID, link.triangle);
								if (reciprocal.triangle == ComputeTriangleIndex(currentID))
								{
									distance = s;
									nextID = possibleNextID;
									possibleIncidentEdge = reciprocal.edge;
									possibleTile = tile;
									possibleTileOrigin = tileOrigin;
									possibleContainer = container;
									possibleTileID = tileID;

									break;   // opposite edge loop
								}
							}
						}
						else if (side != Tile::SLink::OffMesh)
						{
							TileID neighbourTileID = GetNeighbourTileID(container->x, container->y, container->z, link.side);
							const TileContainer& neighbourContainer = m_tiles[neighbourTileID - 1];

							const Tile::STriangle& opposite = neighbourContainer.tile.triangles[link.triangle];

							const uint16 currentTriangleIndex = ComputeTriangleIndex(currentID);
							const uint16 currentOppositeSide = static_cast<uint16>(NavMesh::GetOppositeSide(side));

							for (size_t rl = 0; rl < opposite.linkCount; ++rl)
							{
								const Tile::SLink& reciprocal = neighbourContainer.tile.links[opposite.firstLink + rl];
								if ((reciprocal.triangle == currentTriangleIndex) && (reciprocal.side == currentOppositeSide))
								{
									const vector3_t neighbourTileOrigin = GetTileOrigin(neighbourContainer.x, neighbourContainer.y, neighbourContainer.z);

									const uint16 i0 = reciprocal.edge;
									const uint16 i1 = Utils::next_mod3(reciprocal.edge);

									assert(i0 < 3);
									assert(i1 < 3);

									const vector3_t c = neighbourTileOrigin + vector3_t(
									  neighbourContainer.tile.vertices[opposite.vertex[i0]]);
									const vector3_t d = neighbourTileOrigin + vector3_t(
									  neighbourContainer.tile.vertices[opposite.vertex[i1]]);

									const TriangleID possibleNextID = ComputeTriangleID(neighbourTileID, link.triangle);
									real_t p, q;
									if (Utils::IntersectSegmentSegment(vector2_t(fromLocalPosition), vector2_t(toLocalPosition), vector2_t(c), vector2_t(d), p, q))
									{
										distance = p;
										nextID = possibleNextID;
										possibleIncidentEdge = reciprocal.edge;

										possibleTileID = neighbourTileID;
										possibleContainer = &neighbourContainer;
										possibleTile = &neighbourContainer.tile;

										possibleTileOrigin = neighbourTileOrigin;
									}

									break;   // reciprocal link loop
								}
							}
						}

						if (nextID)
							break;   // link loop
					}

					// Is triangle nextID already in a way?
					if (std::find(raycastRequest.way, raycastRequest.way + triCount, nextID) != (raycastRequest.way + triCount))
					{
						assert(0);
						nextID = TriangleID();
					}

					// If distance is equals to 0, it means that our starting position is placed
					// on an edge or on a shared vertex. This means that we need to continue evaluating
					// the other edges of the triangle to check if we have a better intersection.
					// This will happen if the ray starts from one edge of the triangle but intersects also
					// on of the other two edges. If we stop to the first intersection (the one with 0 distance)
					// we can end up trying to raycast in the wrong direction.
					// As soon as the distance is more than 0 we can stop the evaluation of the other edges
					// because it means we are moving towards the direction of the ray.
					distance = s;
					const bool shouldStopEvaluationOfOtherEdges = distance > 0;
					if (shouldStopEvaluationOfOtherEdges)
					{
						if (nextID)
							break;   // edge loop
						else
						{
							RayHit& hit = raycastRequest.hit;
							hit.distance = distance;
							hit.triangleID = currentID;
							hit.edge = e;

							raycastRequest.wayTriCount = triCount;

							return ERayCastResult::Hit;
						}
					}
				}
			}

			currentID = nextID;
			incidentEdge = possibleIncidentEdge;
			tile = possibleTile ? possibleTile : tile;
			tileID = possibleTileID;
			container = possibleContainer ? possibleContainer : container;
			tileOrigin = possibleTileOrigin;
		}

		raycastRequest.wayTriCount = triCount;

		bool isEndingTriangleAcceptable = QueryIsTriangleAcceptableForLocation(toLocalPosition, raycastRequest.way[triCount - 1]);
		return isEndingTriangleAcceptable ? ERayCastResult::NoHit : ERayCastResult::DisconnectedLocations;
	}

	return ERayCastResult::InvalidStart;
}

#pragma warning(push)
#pragma warning(disable:28285)
bool TestEdgeOverlap1D(const real_t& a0, const real_t& a1,
                       const real_t& b0, const real_t& b1, const real_t& dx)
{
	if ((a0 == b0) && (a1 == b1))
		return true;

	const real_t amin = a0 < a1 ? a0 : a1;
	const real_t amax = a0 < a1 ? a1 : a0;

	const real_t bmin = b0 < b1 ? b0 : b1;
	const real_t bmax = b0 < b1 ? b1 : b0;

	const real_t ominx = std::max(amin + dx, bmin + dx);
	const real_t omaxx = std::min(amax - dx, bmax - dx);

	if (ominx > omaxx)
		return false;

	return true;
}
#pragma warning(pop)

bool TestEdgeOverlap2D(const real_t& toleranceSq, const vector2_t& a0, const vector2_t& a1,
                       const vector2_t& b0, const vector2_t& b1, const real_t& dx)
{
	if ((a0.x == b0.x) && (a1.x == b1.x) && (a0.x == a1.x))
		return TestEdgeOverlap1D(a0.y, a1.y, b0.y, b1.y, dx);

	const vector2_t amin = a0.x < a1.x ? a0 : a1;
	const vector2_t amax = a0.x < a1.x ? a1 : a0;

	const vector2_t bmin = b0.x < b1.x ? b0 : b1;
	const vector2_t bmax = b0.x < b1.x ? b1 : b0;

	const real_t ominx = std::max(amin.x, bmin.x) + dx;
	const real_t omaxx = std::min(amax.x, bmax.x) - dx;

	if (ominx >= omaxx)
		return false;

	const real_t aslope = ((amax.x - amin.x) != 0) ? ((amax.y - amin.y) / (amax.x - amin.x)) : 0;
	const real_t a_cValue = amin.y - aslope * amin.x;

	const real_t bslope = ((bmax.x - bmin.x) != 0) ? ((bmax.y - bmin.y) / (bmax.x - bmin.x)) : 0;
	const real_t b_cValue = bmin.y - bslope * bmin.x;

	const real_t aominy = a_cValue + aslope * ominx;
	const real_t bominy = b_cValue + bslope * ominx;

	const real_t aomaxy = a_cValue + aslope * omaxx;
	const real_t bomaxy = b_cValue + bslope * omaxx;

	const real_t dminy = bominy - aominy;
	const real_t dmaxy = bomaxy - aomaxy;

	if ((square(dminy) > toleranceSq) || (square(dmaxy) > toleranceSq))
	{
		return false;
	}

	return true;
}

bool TestEdgeOverlap(size_t side, const real_t& toleranceSq, const vector3_t& a0, const vector3_t& a1,
                     const vector3_t& b0, const vector3_t& b1)
{
	const int ox = NavMesh::GetNeighbourTileOffset(side)[0];
	const int oy = NavMesh::GetNeighbourTileOffset(side)[1];
	const real_t dx = real_t::fraction(1, 1000);

	if (ox || oy)
	{
		const size_t dim = ox ? 0 : 1;
		const size_t odim = dim ^ 1;

		if ((a1[dim] - a0[dim] != 0) || (b1[dim] - b0[dim] != 0) || (a0[dim] != b0[dim]))
			return false;

		return TestEdgeOverlap2D(toleranceSq, vector2_t(a0[odim], a0.z), vector2_t(a1[odim], a1.z),
		                         vector2_t(b0[odim], b0.z), vector2_t(b1[odim], b1.z), dx);
	}
	else
	{
		if ((a1.z - a0.z != 0) || (b1.z - b0.z != 0) || (a0.z != b0.z))
			return false;

		return TestEdgeOverlap2D(toleranceSq, vector2_t(a0.x, a0.y), vector2_t(a1.x, a1.y),
		                         vector2_t(b0.x, b0.y), vector2_t(b1.x, b1.y), dx);
	}
}

TileID CNavMesh::SetTile(size_t x, size_t y, size_t z, STile& tile)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert((x <= max_x) && (y <= max_y) && (z <= max_z));

	tile.ValidateTriangles();

	const size_t tileName = ComputeTileName(x, y, z);

	std::pair<TileMap::iterator, bool> iresult = m_tileMap.insert(TileMap::value_type(tileName, TileID()));

	TileID tileID;

	if (iresult.second)
	{
		iresult.first->second = tileID = TileID(m_tiles.AllocateTileContainer());

		m_profiler.AddStat(TileCount, 1);
	}
	else
	{
		tileID = iresult.first->second;

		STile& oldTile = m_tiles[tileID - 1].tile;
		m_triangleCount -= oldTile.triangleCount;

		m_profiler.AddStat(VertexCount, -(int)oldTile.vertexCount);
		m_profiler.AddStat(TriangleCount, -(int)oldTile.triangleCount);
		m_profiler.AddStat(BVTreeNodeCount, -(int)oldTile.nodeCount);
		m_profiler.AddStat(LinkCount, -(int)oldTile.linkCount);

		m_profiler.FreeMemory(VertexMemory, oldTile.vertexCount * sizeof(Tile::Vertex));
		m_profiler.FreeMemory(TriangleMemory, oldTile.triangleCount * sizeof(Tile::STriangle));
		m_profiler.FreeMemory(BVTreeMemory, oldTile.nodeCount * sizeof(Tile::SBVNode));
		m_profiler.FreeMemory(LinkMemory, oldTile.linkCount * sizeof(Tile::SLink));

		oldTile.Destroy();
	}

	m_profiler.AddStat(VertexCount, tile.vertexCount);
	m_profiler.AddStat(TriangleCount, tile.triangleCount);
	m_profiler.AddStat(BVTreeNodeCount, tile.nodeCount);
	m_profiler.AddStat(LinkCount, tile.linkCount);

	m_profiler.AddMemory(VertexMemory, tile.vertexCount * sizeof(Tile::Vertex));
	m_profiler.AddMemory(TriangleMemory, tile.triangleCount * sizeof(Tile::STriangle));
	m_profiler.AddMemory(BVTreeMemory, tile.nodeCount * sizeof(Tile::SBVNode));
	m_profiler.AddMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));

	m_profiler.FreeMemory(GridMemory, m_profiler[GridMemory].used);
	m_profiler.AddMemory(GridMemory, m_tileMap.size() * sizeof(TileMap::value_type));
	m_tiles.UpdateProfiler(m_profiler);

	m_triangleCount += tile.triangleCount;

	TileContainer& container = m_tiles[tileID - 1];
	container.x = x;
	container.y = y;
	container.z = z;
	container.tile.Swap(tile);
	tile.Destroy();

	return tileID;
}

void CNavMesh::ClearTile(TileID tileID, bool clearNetwork)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	{
		TileContainer& container = m_tiles[tileID - 1];

		m_profiler.AddStat(VertexCount, -(int)container.tile.vertexCount);
		m_profiler.AddStat(TriangleCount, -(int)container.tile.triangleCount);
		m_profiler.AddStat(BVTreeNodeCount, -(int)container.tile.nodeCount);
		m_profiler.AddStat(LinkCount, -(int)container.tile.linkCount);

		m_profiler.FreeMemory(VertexMemory, container.tile.vertexCount * sizeof(Tile::Vertex));
		m_profiler.FreeMemory(TriangleMemory, container.tile.triangleCount * sizeof(Tile::STriangle));
		m_profiler.FreeMemory(BVTreeMemory, container.tile.nodeCount * sizeof(Tile::SBVNode));
		m_profiler.FreeMemory(LinkMemory, container.tile.linkCount * sizeof(Tile::SLink));

		m_profiler.AddStat(TileCount, -1);

		m_triangleCount -= container.tile.triangleCount;

		m_tiles.FreeTileContainer(tileID);

		container.tile.Destroy();

		TileMap::iterator it = m_tileMap.find(ComputeTileName(container.x, container.y, container.z));
		assert(it != m_tileMap.end());
		m_tileMap.erase(it);

		if (clearNetwork)
		{
			for (size_t side = 0; side < NavMesh::SideCount; ++side)
			{
				size_t nx = container.x + NavMesh::GetNeighbourTileOffset(side)[0];
				size_t ny = container.y + NavMesh::GetNeighbourTileOffset(side)[1];
				size_t nz = container.z + NavMesh::GetNeighbourTileOffset(side)[2];

				if (TileID neighbourID = GetTileID(nx, ny, nz))
				{
					TileContainer& ncontainer = m_tiles[neighbourID - 1];

					ReComputeAdjacency(ncontainer.x, ncontainer.y, ncontainer.z, kAdjecencyCalculationToleranceSq, ncontainer.tile,
						NavMesh::GetOppositeSide(side), container.x, container.y, container.z, tileID);
				}
			}
		}

		m_profiler.FreeMemory(GridMemory, m_profiler[GridMemory].used);
		m_profiler.AddMemory(GridMemory, m_tileMap.size() * sizeof(TileMap::value_type));
		m_tiles.UpdateProfiler(m_profiler);
	}
}

void CNavMesh::CreateNetwork()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	TileMap::iterator it = m_tileMap.begin();
	TileMap::iterator end = m_tileMap.end();

	const real_t toleranceSq = square(real_t(std::max(m_params.voxelSize.x, m_params.voxelSize.z)));
	const MNM::CTileConnectivityData* pNullConnectivityData = nullptr;

	for (; it != end; ++it)
	{
		const TileID tileID = it->second;

		TileContainer& container = m_tiles[tileID - 1];
		ComputeAdjacency(container.x, container.y, container.z, toleranceSq, container.tile, pNullConnectivityData);
	}
}

void CNavMesh::ConnectToNetwork(const TileID tileID, const CTileConnectivityData* pConnectivityData)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	TileContainer& container = m_tiles[tileID - 1];

	ComputeAdjacency(container.x, container.y, container.z, kAdjecencyCalculationToleranceSq, container.tile, pConnectivityData);

	for (size_t side = 0; side < NavMesh::SideCount; ++side)
	{
		const size_t nx = container.x + NavMesh::GetNeighbourTileOffset(side)[0];
		const size_t ny = container.y + NavMesh::GetNeighbourTileOffset(side)[1];
		const size_t nz = container.z + NavMesh::GetNeighbourTileOffset(side)[2];

		if (TileID neighbourID = GetTileID(nx, ny, nz))
		{
			TileContainer& ncontainer = m_tiles[neighbourID - 1];

			ReComputeAdjacency(ncontainer.x, ncontainer.y, ncontainer.z, kAdjecencyCalculationToleranceSq, ncontainer.tile,
				NavMesh::GetOppositeSide(side), container.x, container.y, container.z, tileID);
		}
	}
}

TileID CNavMesh::GetTileID(size_t x, size_t y, size_t z) const
{
	const size_t tileName = ComputeTileName(x, y, z);

	TileMap::const_iterator it = m_tileMap.find(tileName);
	if (it != m_tileMap.end())
		return it->second;
	return TileID();
}

const STile& CNavMesh::GetTile(TileID tileID) const
{
	assert(tileID > 0);
	return m_tiles[tileID - 1].tile;
}

STile& CNavMesh::GetTile(TileID tileID)
{
	assert(tileID > 0);
	return m_tiles[tileID - 1].tile;
}

const vector3_t CNavMesh::GetTileContainerCoordinates(TileID tileID) const
{
	assert(tileID > 0);
	const TileContainer& container = m_tiles[tileID - 1];
	return vector3_t(MNM::real_t(container.x), MNM::real_t(container.y), MNM::real_t(container.z));
}

void CNavMesh::Swap(CNavMesh& other)
{
	m_tiles.Swap(other.m_tiles);
	std::swap(m_triangleCount, other.m_triangleCount);

	m_tileMap.swap(other.m_tileMap);

	std::swap(m_params, other.m_params);
	std::swap(m_profiler, other.m_profiler);
}

void CNavMesh::Draw(size_t drawFlags, const ITriangleColorSelector& colorSelector, TileID excludeID) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const CCamera& camera = gEnv->pSystem->GetViewCamera();
	const Vec3 cameraPos = camera.GetPosition();
	const float maxDistanceToRenderSqr = sqr(gAIEnv.CVars.navigation.NavmeshTileDistanceDraw);

	// collect areas
	// TODO: Clean this up! Temporary to get up and running.
	std::vector<float> islandAreas(m_islands.GetTotalIslands());
	for (size_t i = 0; i < m_islands.GetTotalIslands(); ++i)
	{
		islandAreas[i] = m_islands.GetIslandArea(i + 1);
	}

	TileMap::const_iterator it = m_tileMap.begin();
	TileMap::const_iterator end = m_tileMap.end();
	for (; it != end; ++it)
	{
		if (excludeID == it->second)
			continue;

		const TileContainer& container = m_tiles[it->second - 1];
		const vector3_t globalTileOrigin = vector3_t(m_params.origin) + GetTileOrigin(container.x, container.y, container.z);

		const Vec3 min = globalTileOrigin.GetVec3();
		if (cameraPos.GetSquaredDistance2D(min) < maxDistanceToRenderSqr && camera.IsAABBVisible_F(AABB(min, min + m_params.tileSize)))
		{
			container.tile.Draw(drawFlags, globalTileOrigin, it->second, islandAreas, colorSelector);
		}
	}
}

const CNavMesh::ProfilerType& CNavMesh::GetProfiler() const
{
	return m_profiler;
}

TileID CNavMesh::GetNeighbourTileID(size_t x, size_t y, size_t z, size_t side) const
{
	size_t nx = x + NavMesh::GetNeighbourTileOffset(side)[0];
	size_t ny = y + NavMesh::GetNeighbourTileOffset(side)[1];
	size_t nz = z + NavMesh::GetNeighbourTileOffset(side)[2];

	return GetTileID(nx, ny, nz);
}

TileID CNavMesh::GetNeighbourTileID(const TileID tileId, size_t side) const
{
	CRY_ASSERT(tileId > 0);
	const TileContainer& container = m_tiles[tileId - 1];
	return GetNeighbourTileID(container.x, container.y, container.z, side);
}

NavigationMeshID CNavMesh::GetMeshId() const 
{
	return m_meshId;
}

void CNavMesh::GetMeshParams(NavMesh::SParams& outParams) const
{
	const SGridParams& params = GetGridParams();
	outParams.originWorld = params.origin;
}

TileID CNavMesh::FindTileIDByTileGridCoord(const vector3_t& tileGridCoord) const
{
	const int nx = tileGridCoord.x.as_int();
	const int ny = tileGridCoord.y.as_int();
	const int nz = tileGridCoord.z.as_int();

	if (nx < 0 || ny < 0 || nz < 0)
	{
		return TileID();
	}

	return GetTileID(nx, ny, nz);
}

MNM::SClosestTriangle CNavMesh::FindClosestTriangle(const vector3_t& localPosition, const DynArray<TriangleID>& candidateTriangles) const
{
	CNearestTriangleQueryProcessing queryProcessing(m_meshId, localPosition);
	queryProcessing.AddTriangleArray(candidateTriangles);
	queryProcessing.Process();

	return MNM::SClosestTriangle(
		queryProcessing.GetClosestTriangleId(),
		queryProcessing.GetClosestPosition(),
		queryProcessing.GetClosestDistance()
	);
}

bool CNavMesh::GetTileData(const TileID tileId, Tile::STileData& outTileData) const
{
	if (tileId)
	{
		const TileContainer& container = m_tiles[tileId - 1];

		const vector3_t tileOrigin = GetTileOrigin(container.x, container.y, container.z);

		container.tile.GetTileData(outTileData);
		outTileData.tileGridCoord = vector3_t(
		  real_t(container.x),
		  real_t(container.y),
		  real_t(container.z));
		outTileData.tileOriginWorld = tileOrigin + vector3_t(m_params.origin);

		return true;
	}
	return false;
}

struct SideTileInfo
{
	SideTileInfo()
		: tile(0)
	{
	}

	STile*    tile;
	vector3_t offset;
};

size_t CreateEdgeLinksWithNeighbors(const SideTileInfo* pSides, const uint16 sidesMask, const vector3_t edgeVertex0, const vector3_t edgeVertex1, const uint16 edgeIdx, const real_t toleranceSq, Tile::SLink* pLinks, size_t& linksCount)
{
	size_t addedLinks = 0;
	
	for (size_t sideIdx = 0; sideIdx < NavMesh::SideCount; ++sideIdx)
	{
		if((sidesMask & BIT16(sideIdx)) == 0)
			continue;
		
		const STile* pNeighbourTile = pSides[sideIdx].tile;
		if(!pNeighbourTile)
			continue;

		const vector3_t& offset = pSides[sideIdx].offset;

		const size_t neighborTrianglesCount = pNeighbourTile->GetTrianglesCount();
		const Tile::STriangle* pNeighborTriangles = pNeighbourTile->GetTriangles();
		const Tile::Vertex* pNeighborVertices = pNeighbourTile->GetVertices();

		for (size_t neiTriangleIdx = 0; neiTriangleIdx < neighborTrianglesCount; ++neiTriangleIdx)
		{
			const Tile::STriangle& neighborTriangle = pNeighborTriangles[neiTriangleIdx];

			for (size_t neiEdgeIdx = 0; neiEdgeIdx < 3; ++neiEdgeIdx)
			{
				const vector3_t neiEdgeVertex0 = offset + vector3_t(pNeighborVertices[neighborTriangle.vertex[neiEdgeIdx]]);
				const vector3_t neiEdgeVertex1 = offset + vector3_t(pNeighborVertices[neighborTriangle.vertex[Utils::next_mod3(neiEdgeIdx)]]);

				if (TestEdgeOverlap(sideIdx, toleranceSq, edgeVertex0, edgeVertex1, neiEdgeVertex0, neiEdgeVertex1))
				{
					Tile::SLink& link = pLinks[linksCount++];
					link.side = sideIdx;
					link.edge = edgeIdx;
					link.triangle = neiTriangleIdx;

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
					const TileID checkId = GetTileID(x + NavMesh::GetNeighbourTileOffset(sideIdx)[0], y + NavMesh::GetNeighbourTileOffset(sideIdx)[1], z + NavMesh::GetNeighbourTileOffset(sideIdx)[2]);
					m_tiles.BreakOnInvalidTriangle(ComputeTriangleID(checkId, neiTriangleIdx));
#endif

					++addedLinks;
					break;
				}
			}
		}
	}
	return addedLinks;
}

#pragma warning (push)
#pragma warning (disable: 6262)
void CNavMesh::ComputeAdjacency(size_t x, size_t y, size_t z, const real_t& toleranceSq, STile& tile, const CTileConnectivityData* pConnectivityData)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const size_t vertexCount = tile.vertexCount;
	const Tile::Vertex* vertices = tile.GetVertices();

	const size_t triCount = tile.triangleCount;
	if (!triCount)
		return;

	m_profiler.StartTimer(NetworkConstruction);

	CRY_ASSERT(triCount <= MNM::Constants::TileTrianglesMaxCount);

	const size_t MaxLinkCount = MNM::Constants::TileTrianglesMaxCount * 6;
	Tile::SLink links[MaxLinkCount];
	size_t linkCount = 0;

	SideTileInfo sides[NavMesh::SideCount];
	for (size_t s = 0; s < NavMesh::SideCount; ++s)
	{
		SideTileInfo& side = sides[s];

		if (const TileID id = GetTileID(x + NavMesh::GetNeighbourTileOffset(s)[0], y + NavMesh::GetNeighbourTileOffset(s)[1], z + NavMesh::GetNeighbourTileOffset(s)[2]))
		{
			side.tile = &GetTile(id);
			side.offset = vector3_t(
			  NavMesh::GetNeighbourTileOffset(s)[0] * m_params.tileSize.x,
			  NavMesh::GetNeighbourTileOffset(s)[1] * m_params.tileSize.y,
			  NavMesh::GetNeighbourTileOffset(s)[2] * m_params.tileSize.z);
		}
	}

	if (!pConnectivityData)
	{
		// There are no precomputed adjacency data - generate them now
		CTileConnectivityData::Edge edges[MNM::Constants::TileTrianglesMaxCount * 3];
		uint16 adjacency[MNM::Constants::TileTrianglesMaxCount * 3];
		CTileConnectivityData::ComputeTriangleAdjacency(tile.GetTriangles(), triCount, tile.GetVertices(), vertexCount, vector3_t(m_params.tileSize), *&edges, *&adjacency);

		for (size_t triangleIdx = 0; triangleIdx < triCount; ++triangleIdx)
		{
			size_t triLinkCount = 0;

			for (uint16 edgeIdx = 0; edgeIdx < 3; ++edgeIdx)
			{
				const size_t edgeIndex = adjacency[CTileConnectivityData::GetAdjancencyIdx(triangleIdx, edgeIdx)];
				const CTileConnectivityData::Edge& edge = edges[edgeIndex];
				if (edge.IsInternalEdgeOfTriangle(triangleIdx))
				{
					Tile::SLink& link = links[linkCount++];
					link.side = Tile::SLink::Internal;
					link.edge = edgeIdx;
					link.triangle = (edge.triangleIndex[1] == triangleIdx) ? edge.triangleIndex[0] : edge.triangleIndex[1];

					++triLinkCount;
				}
			}

			if (triLinkCount < 3)
			{
				for (uint16 edgeIdx = 0; edgeIdx < 3; ++edgeIdx)
				{
					const size_t edgeIndex = adjacency[triangleIdx * 3 + edgeIdx];
					const CTileConnectivityData::Edge& edge = edges[edgeIndex];
					if (edge.IsBoundaryEdgeOfTriangle(triangleIdx))
					{
						const vector3_t a0 = vector3_t(vertices[edge.vertexIndex[0]]);
						const vector3_t a1 = vector3_t(vertices[edge.vertexIndex[1]]);
						triLinkCount += CreateEdgeLinksWithNeighbors(sides, edge.sidesToCheckMask, a0, a1, edgeIdx, toleranceSq, links, linkCount);
					}
				}
			}

			Tile::STriangle& triangle = tile.triangles[triangleIdx];
			triangle.linkCount = triLinkCount;
			triangle.firstLink = linkCount - triLinkCount;
		}
	}
	else
	{
		// There are precomputed adjacency data - use them.
		// This also means that the tile internal links were already generated, so we just need to copy them and then create links to neighbor tiles		
		for (size_t triangleIdx = 0; triangleIdx < triCount; ++triangleIdx)
		{
			Tile::STriangle& triangle = tile.triangles[triangleIdx];
			size_t triLinksCount = triangle.linkCount;
			for (size_t linkIdx = triangle.firstLink, linkend = triangle.firstLink + triLinksCount; linkIdx < linkend; ++linkIdx)
			{
				CRY_ASSERT(tile.links[linkIdx].side == Tile::SLink::Internal);
				links[linkCount++] = tile.links[linkIdx];
			}

			if (triangle.linkCount < 3)
			{
				for (uint16 edgeIdx = 0; edgeIdx < 3; ++edgeIdx)
				{
					const CTileConnectivityData::Edge& edge = pConnectivityData->GetEdge(triangleIdx, edgeIdx);
					if (edge.IsBoundaryEdgeOfTriangle(triangleIdx))
					{
						const vector3_t a0 = vector3_t(vertices[edge.vertexIndex[0]]);
						const vector3_t a1 = vector3_t(vertices[edge.vertexIndex[1]]);
						triLinksCount += CreateEdgeLinksWithNeighbors(sides, edge.sidesToCheckMask, a0, a1, edgeIdx, toleranceSq, links, linkCount);
					}
				}
			}
			triangle.linkCount = triLinksCount;
			triangle.firstLink = linkCount - triLinksCount;
		}
	}

	m_profiler.FreeMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));
	m_profiler.AddStat(LinkCount, -(int)tile.linkCount);

	tile.CopyLinks(links, static_cast<uint16>(linkCount));

	m_profiler.AddMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));
	m_profiler.AddStat(LinkCount, tile.linkCount);
	m_profiler.StopTimer(NetworkConstruction);
}
#pragma warning (pop)

void CNavMesh::ReComputeAdjacency(size_t x, size_t y, size_t z, const real_t& toleranceSq, STile& tile,
                                  size_t side, size_t tx, size_t ty, size_t tz, TileID targetID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const Tile::Vertex* vertices = tile.GetVertices();

	const size_t originTriangleCount = tile.triangleCount;
	if (!originTriangleCount)
		return;

	m_profiler.StartTimer(NetworkConstruction);

	if (!tile.linkCount)
	{
		const MNM::CTileConnectivityData* pNullConnectivityData = nullptr;
		ComputeAdjacency(x, y, z, toleranceSq, tile, pNullConnectivityData);
	}
	else
	{
		const vector3_t noffset = vector3_t(
		  NavMesh::GetNeighbourTileOffset(side)[0] * m_params.tileSize.x,
		  NavMesh::GetNeighbourTileOffset(side)[1] * m_params.tileSize.y,
		  NavMesh::GetNeighbourTileOffset(side)[2] * m_params.tileSize.z);

		CRY_ASSERT(originTriangleCount <= MNM::Constants::TileTrianglesMaxCount);

		const size_t MaxLinkCount = MNM::Constants::TileTrianglesMaxCount * 6;
		Tile::SLink links[MaxLinkCount];
		size_t linkCount = 0;

		const STile* targetTile = targetID ? &m_tiles[targetID - 1].tile : 0;

		const Tile::Vertex* targetVertices = targetTile ? targetTile->GetVertices() : 0;

		const size_t targetTriangleCount = targetTile ? targetTile->triangleCount : 0;
		const Tile::STriangle* targetTriangles = targetTile ? targetTile->GetTriangles() : 0;

		for (size_t i = 0; i < originTriangleCount; ++i)
		{
			size_t triLinkCount = 0;
			size_t triLinkCountI = 0;

			Tile::STriangle& originTriangle = tile.triangles[i];
			const size_t originLinkCount = originTriangle.linkCount;
			const size_t originFirstLink = originTriangle.firstLink;

			for (size_t l = 0; l < originLinkCount; ++l)
			{
				const Tile::SLink& originLink = tile.links[originFirstLink + l];

				if ((originLink.side == Tile::SLink::Internal) || (originLink.side != side))
				{
					links[linkCount++] = originLink;
					++triLinkCount;
					triLinkCountI += (originLink.side == Tile::SLink::Internal) ? 1 : 0;
				}
			}

			if ((triLinkCountI == 3) || !targetID)
			{
				originTriangle.linkCount = triLinkCount;
				originTriangle.firstLink = linkCount - triLinkCount;

				continue;
			}

			for (size_t e = 0; e < 3; ++e)
			{
				const vector3_t a0 = vector3_t(vertices[originTriangle.vertex[e]]);
				const vector3_t a1 = vector3_t(vertices[originTriangle.vertex[Utils::next_mod3(e)]]);

				for (size_t k = 0; k < targetTriangleCount; ++k)
				{
					const Tile::STriangle& ttriangle = targetTriangles[k];

					for (size_t ne = 0; ne < 3; ++ne)
					{
						const vector3_t b0 = noffset + vector3_t(targetVertices[ttriangle.vertex[ne]]);
						const vector3_t b1 = noffset + vector3_t(targetVertices[ttriangle.vertex[Utils::next_mod3(ne)]]);

						if (TestEdgeOverlap(side, toleranceSq, a0, a1, b0, b1))
						{
							Tile::SLink& link = links[linkCount++];
							link.side = side;
							link.edge = e;
							link.triangle = k;
							++triLinkCount;
#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
							const TileID checkId = GetTileID(x + NavMesh::GetNeighbourTileOffset(side)[0], y + NavMesh::GetNeighbourTileOffset(side)[1], z + NavMesh::GetNeighbourTileOffset(side)[2]);
							m_tiles.BreakOnInvalidTriangle(ComputeTriangleID(checkId, k));
							/*
							   Tile::Link testLink;
							   testLink.side = side;
							   testLink.edge = e;
							   testLink.triangle = k;
							   BreakOnMultipleAdjacencyLinkage(links, links+linkCount, testLink);
							 */
#endif
							break;
						}
					}
				}
			}

			originTriangle.linkCount = triLinkCount;
			originTriangle.firstLink = linkCount - triLinkCount;
		}

		m_profiler.FreeMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));
		m_profiler.AddStat(LinkCount, -(int)tile.linkCount);

		tile.CopyLinks(links, static_cast<uint16>(linkCount));

		m_profiler.AddMemory(LinkMemory, tile.linkCount * sizeof(Tile::SLink));
		m_profiler.AddStat(LinkCount, tile.linkCount);
	}

	m_profiler.StopTimer(NetworkConstruction);
}

bool CNavMesh::CalculateMidEdge(const TriangleID triangleID1, const TriangleID triangleID2, Vec3& result) const
{
	if (triangleID1 == triangleID2)
		return false;

	if (const TileID tileID = ComputeTileID(triangleID1))
	{
		const TileContainer& container = m_tiles[tileID - 1];
		const STile& tile = container.tile;
		const uint16 triangleIdx = ComputeTriangleIndex(triangleID1);
		const Tile::STriangle& triangle = tile.triangles[triangleIdx];

		uint16 vi0 = 0, vi1 = 0;

		for (size_t l = 0; l < triangle.linkCount; ++l)
		{
			const Tile::SLink& link = tile.links[triangle.firstLink + l];

			if (link.side == Tile::SLink::Internal)
			{
				TriangleID linkedTriID = ComputeTriangleID(tileID, link.triangle);
				if (linkedTriID == triangleID2)
				{
					vi0 = link.edge;
					vi1 = (link.edge + 1) % 3;
					assert(vi0 < 3);
					assert(vi1 < 3);
					break;
				}
			}
			else if (link.side != Tile::SLink::OffMesh)
			{
				TileID neighbourTileID = GetNeighbourTileID(container.x, container.y, container.z, link.side);
				TriangleID linkedTriID = ComputeTriangleID(neighbourTileID, link.triangle);
				if (linkedTriID == triangleID2)
				{
					vi0 = link.edge;
					vi1 = (link.edge + 1) % 3;
					assert(vi0 < 3);
					assert(vi1 < 3);
					break;
				}
			}
		}

		if (vi0 != vi1)
		{
			vector3_t v0, v1, v2;
			GetVertices(triangleID1, v0, v1, v2);
			vector3_t vertices[3] = { v0, v1, v2 };

			PREFAST_ASSUME(vi0 < 3 && vi1 < 3);
			result = (vertices[vi0] + vertices[vi1]).GetVec3() * 0.5f;
			return true;
		}
	}

	return false;
}

bool CNavMesh::FindNextIntersectingTriangleEdge(const vector3_t& rayStartPos3D, const vector3_t& rayEndPos3D, const vector2_t pTriangleVertices[3],
	real_t& rayIntersectionParam, uint16& intersectingEdgeIndex) const
{
	/*       
	          _Rs
	        \ |
	         \|       /           Rs, Re - ray start, ray end
	          \i2    /            V0, V1, V2 - triangle vertices
	          |\    /             e0, e1, e2 - triangle edges
	          | \  /              i0, i1, i2 - intersections of the ray with edges
	          |  \/V0             
	          |  /\               Assumption: All triangles in navmesh have the same orientation
	          | /  \              
	          |/    \             We are iterating over all edges and trying to find the one, through which the ray is leaving the triangle
	          /i0    \            It is known that |a x b| = |a|*|b|*sinT
	         /|       \ e2        We then compute 'det' as magnitude of the cross product between ray direction and edge, then if
	     e0 / |        \          det == 0: Ray and edge are parallel - ray is either outside of the triangle or there is another edge we are interested in
	       /  |         \         det < 0: Ray is entering a half-plane where the triangle is located
	   ___/___|__________\____    det > 0: Ray is leaving a half-plane where the triangle is located - we are interested only in this case
	   V1/    |i1   e1    \V2              Next we need to find the nearest of these intersections (as minimum intersection parameter of the ray),
	    /     |            \               that gives us the actual 'leaving' edge.  
	          |                            There is also needed check whether the intersection really lies on the edge (mainly because of precision and rounding errors)
	          V             
	           Re                 When none of the values of 'leaving' intersection parameters are smaller than 1.0, it means, that the ray is ending inside the triangle.
	                              Note: Relation between 'det' and intersection parameters is described in DetailedIntersectSegmentSegment function.
	*/
	
	// We are interested in finding edges intersecting the ray where the intersection param minIntersectionParam (p) should be less or equal 1.0 ( I = p * (Re - Rs)), 
	// but due to numerical inaccuracy we set initial value slightly bigger than 1.0 to make sure to find the edge where the ray should be leaving (or is ending).
	// Meaning of the minIntersectionParam values at the end of the function:
	// minIntersectionParam < 1.0 : ray is going outside of the triangle, neighbor triangle will be checked next.
	// minIntersectionParam >= 1.0 : ray is ending inside or directly on the edge (or corner) of the triangle, ray won't go further. 
	real_t minIntersectionParam(1.05f);
	intersectingEdgeIndex = uint16(MNM::Constants::InvalidEdgeIndex);

	const vector2_t rayStartPos = vector2_t(rayStartPos3D);
	const vector2_t rayEndPos = vector2_t(rayEndPos3D);
	const vector2_t rayDir = rayEndPos - rayStartPos;
	const real_t tolerance = real_t::epsilon();
	const real_t minAllowedValue = real_t(0.0f) - tolerance;

	bool isEndingInside = true;

	for (uint16 edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
	{
		const vector2_t& edgeStartPos = pTriangleVertices[edgeIndex];
		const vector2_t& edgeEndPos = pTriangleVertices[inc_mod3[edgeIndex]];

		const vector2_t edgeDir = edgeEndPos - edgeStartPos;
		const vector2_t diff = edgeStartPos - rayStartPos;

		const real_t det = rayDir.cross(edgeDir);
		const real_t n = diff.cross(edgeDir);

		if (det > 0)
		{
			// Ray is possibly leaving the triangle through this edge line

			// Modified version of n/det < minIntersectionParam condition because signed division in fixed point aritmetics is not reliable for small values of divisor
			// (dividing positive number with negative can lead to positive result)
			if (n <= minIntersectionParam * det)
			{
				isEndingInside = false;

				// Make sure that the intersections really lies on the edge segment (only checking for the nearest intersection isn't enough because of precision and rounding errors)
				const real_t m = diff.cross(rayDir);
				const real_t maxAllowedValue = det + tolerance;

				if (m >= minAllowedValue && m <= maxAllowedValue)
				{
					minIntersectionParam = n / det;
					intersectingEdgeIndex = edgeIndex;
				}
			}
		}
		else if (det == 0 && n < minAllowedValue)
		{
			// Ray segment is parallel to this edge and is completely outside of the triangle
			isEndingInside = false;
		}
	}

	rayIntersectionParam = minIntersectionParam;

	return isEndingInside;
}

uint16 GetNearestTriangleVertexIndex(const STile& tile, const vector3_t& tileOffset, const Tile::STriangle& triangle, const vector3_t& position, const uint16 edgeIndex)
{
	const vector3_t targetPositionOffset = position - tileOffset;
	const uint16 vertexIdx1 = edgeIndex;
	const uint16 vertexIdx2 = inc_mod3[edgeIndex];
	const vector3_t& vertex1 = tile.GetVertices()[triangle.vertex[vertexIdx1]];
	const vector3_t& vertex2 = tile.GetVertices()[triangle.vertex[vertexIdx2]];

	return vector2_t(vertex1 - targetPositionOffset).lenSq() <= vector2_t(vertex2 - targetPositionOffset).lenSq() ? vertexIdx1 : vertexIdx2;
}

bool CNavMesh::IsPointInsideNeighbouringTriangleWhichSharesEdge(const TriangleID sourceTriangleID, const uint16 sourceEdgeIndex, const vector3_t& targetPosition, const TriangleID targetTriangleID) const
{
	const TileID sourceTileID = ComputeTileID(sourceTriangleID);
	const TileContainer& sourceContainer = m_tiles[sourceTileID - 1];
	const STile& sourceTile = sourceContainer.tile;

	const uint16 sourceTriangleIndex = ComputeTriangleIndex(sourceTriangleID);
	const Tile::STriangle& sourceTriangle = sourceTile.triangles[sourceTriangleIndex];

	TriangleID currentTriangleID = sourceTriangleID;
	uint16 currentEdgeIndex = sourceEdgeIndex;
	
	// Find the nearest vertex that should be common for all triangles
	uint16 currentVertexIndex = GetNearestTriangleVertexIndex(sourceTile, GetTileOrigin(sourceContainer.x, sourceContainer.y, sourceContainer.z), sourceTriangle, targetPosition, currentEdgeIndex);
	uint16 vertexIndexInTile = sourceTriangle.vertex[currentVertexIndex];

	const int32 clockwiseEdgeIndices[] = { 0, 1, 2 };
	const int32* counterClockwiseEdgeIndices = dec_mod3;

	// vertexToEdgeIdx is used to determine in which 'direction' triangles should be traversed around the nearest vertex (clockwise or counter-clockwise).
	const int32* vertexToEdgeIdx = currentVertexIndex == currentEdgeIndex ? clockwiseEdgeIndices : counterClockwiseEdgeIndices;

	bool isSwitchingSide = false;
	bool alreadySwitchedSide = false;

	int bailOutCounter = 0;

	// In the case when targetTriangle and sourceTriangle share whole edge, the loop should always break during the first execution
	do
	{
		const TileID currentTileID = ComputeTileID(currentTriangleID);
		const TileContainer& currentContainer = m_tiles[currentTileID - 1];
		const STile& currentTile = currentContainer.tile;

		const uint16 currentTriangleIndex = ComputeTriangleIndex(currentTriangleID);
		const Tile::STriangle& currentTriangle = currentTile.triangles[currentTriangleIndex];

		TriangleID prevTriangleID = currentTriangleID;

		for (size_t linkIndex = 0; linkIndex < currentTriangle.linkCount; ++linkIndex)
		{
			// Find link to neighbour triangle corresponding to found edge
			const Tile::SLink& link = currentTile.links[currentTriangle.firstLink + linkIndex];
			if (link.edge != currentEdgeIndex)
				continue;

			if (link.side == Tile::SLink::OffMesh)
				continue;

			TriangleID neighbourTriangleID;

			if (link.side == Tile::SLink::Internal)
			{
				// Internal link between two triangles in the same tile
				// Next edge is found based on the index of the common vertex in the tile
				neighbourTriangleID = ComputeTriangleID(currentTileID, link.triangle);

				if (neighbourTriangleID == targetTriangleID)
					return true;

				bool edgeIsFound = false;
				const Tile::STriangle& neighbourTriangle = currentTile.triangles[link.triangle];
				for (uint16 neighbourVertexIdx = 0; neighbourVertexIdx < 3; ++neighbourVertexIdx)
				{
					if (neighbourTriangle.vertex[neighbourVertexIdx] == vertexIndexInTile)
					{
						currentEdgeIndex = vertexToEdgeIdx[neighbourVertexIdx];
						edgeIsFound = true;
						break;
					}
				}
				CRY_ASSERT(edgeIsFound);
			}
			else
			{
				// Link connecting triangles in two different tiles
				// Next edge is found by finding the nearest common vertex again
				const TileID neighbourTileID = GetNeighbourTileID(currentContainer.x, currentContainer.y, currentContainer.z, link.side);
				neighbourTriangleID = ComputeTriangleID(neighbourTileID, link.triangle);

				if (neighbourTriangleID == targetTriangleID)
					return true;

				const TileContainer& neighbourContainer = m_tiles[neighbourTileID - 1];
				const STile& neighbourTile = neighbourContainer.tile;
				const Tile::STriangle& neighbourTriangle = neighbourTile.triangles[link.triangle];

				bool edgeIsFound = false;
				for (size_t reciprocalLinkIndex = 0; reciprocalLinkIndex < neighbourTriangle.linkCount; ++reciprocalLinkIndex)
				{
					const Tile::SLink& reciprocalLink = neighbourTile.links[neighbourTriangle.firstLink + reciprocalLinkIndex];
					if (reciprocalLink.side == Tile::SLink::Internal || reciprocalLink.side == Tile::SLink::OffMesh)
						continue;

					const TileID backTileID = GetNeighbourTileID(neighbourContainer.x, neighbourContainer.y, neighbourContainer.z, reciprocalLink.side);
					const TriangleID backTriangleID = ComputeTriangleID(backTileID, reciprocalLink.triangle);

					if (backTriangleID == currentTriangleID)
					{
						currentVertexIndex = GetNearestTriangleVertexIndex(neighbourTile, GetTileOrigin(neighbourContainer.x, neighbourContainer.y, neighbourContainer.z), neighbourTriangle, targetPosition, reciprocalLink.edge);
						vertexIndexInTile = neighbourTriangle.vertex[currentVertexIndex];
						currentEdgeIndex = vertexToEdgeIdx[currentVertexIndex];

						edgeIsFound = true;
						break;
					}
				}
				CRY_ASSERT(edgeIsFound);
			}
			currentTriangleID = neighbourTriangleID;
			break;
		}

		if (prevTriangleID == currentTriangleID)
		{
			if (alreadySwitchedSide)
				return false; // NavMesh boundary was hit from both sides and targetTriangle wasn't found
			
			// It wasn't possible to step through current edge, inverse the side
			vertexToEdgeIdx = clockwiseEdgeIndices == vertexToEdgeIdx ? counterClockwiseEdgeIndices : clockwiseEdgeIndices;
			currentEdgeIndex = vertexToEdgeIdx[sourceEdgeIndex];
			currentTriangleID = sourceTriangleID;
			currentVertexIndex = GetNearestTriangleVertexIndex(sourceTile, GetTileOrigin(sourceContainer.x, sourceContainer.y, sourceContainer.z), sourceTriangle, targetPosition, currentEdgeIndex);
			vertexIndexInTile = sourceTriangle.vertex[currentVertexIndex];

			alreadySwitchedSide = true;
			isSwitchingSide = true;
		}
		else
		{
			isSwitchingSide = false;
		}
	} 
	while ((currentTriangleID != sourceTriangleID || isSwitchingSide) && (++bailOutCounter < 1024));

	CRY_ASSERT_MESSAGE(
		bailOutCounter < 1024, 
		"Potential infinite loop detected: source triangle ID: %u, edge idx: %u, target triangle ID: %u, position: %f, %f, %f", 
		sourceTriangleID, sourceEdgeIndex, targetTriangleID, targetPosition.x.as_float(), targetPosition.y.as_double(), targetPosition.z.as_float());
	return false;
}

}
