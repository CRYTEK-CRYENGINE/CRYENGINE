// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavMesh.h"
#include "OffGridLinks.h"
#include "Tile.h"
#include "../NavigationSystem/OffMeshNavigationManager.h"

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
bool CNavMesh::WayQueryRequest::CanUseOffMeshLink(const OffMeshLinkID linkID, float* costMultiplier) const
{
	if (m_requesterEntityId)
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_requesterEntityId))
		{
			if (const OffMeshLink* pOffMeshLink = m_offMeshNavigationManager.GetOffMeshLink(linkID))
			{
				return pOffMeshLink->CanUse(pEntity, costMultiplier);
			}
		}
	}
	return true;    // Always allow by default
}

bool CNavMesh::WayQueryRequest::IsPointValidForAgent(const Vec3& pos, uint32 flags) const
{
	// TODO: Do we need replacement for this function before introducing new 'QueryFilter'? 
	// All known implementations of m_pRequester (IAIPathAgent) were returning true in IsPointValidForAgent method.
	/*if (m_pRequester)
	{
		return m_pRequester->IsPointValidForAgent(pos, flags);
	}
	*/
	return true;    // Always allow by default
}

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

	size_t tileID;

	if (m_freeIndexes.empty())
	{
		tileID = m_tileCount;

		if (m_tileCount > m_tileCapacity)
			Grow(std::max<size_t>(4, m_tileCapacity >> 1));
	}
	else
	{
		tileID = m_freeIndexes.back() + 1;
		m_freeIndexes.pop_back();
	}

	return static_cast<TileID>(tileID);
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

void CNavMesh::Init(const SGridParams& params)
{
	m_params = params;

	m_tiles.Init(params.tileCount);
}

//! Filter to support old GetTriangles() function (and such), which allow to specify minIslandArea, without NavMesh query filter
struct CNavMesh::SMinIslandAreaQueryTrianglesFilter
{
	SMinIslandAreaQueryTrianglesFilter(const CNavMesh& navMesh_, float minIslandArea_)
		: navMesh(navMesh_)
		, minIslandArea(minIslandArea_)
	{}

	bool PassFilter(const Tile::STriangle& triangle) const
	{
		return navMesh.GetIslands().GetIslandArea(triangle.islandID) >= minIslandArea;
	}

	const CNavMesh& navMesh;
	float           minIslandArea;
};

//! Filter to support old GetTriangles() function (and such), which allow to specify minIslandArea, with NavMesh query filter
struct SNavigationQueryFilterWithMinIslandFilter
{
	SNavigationQueryFilterWithMinIslandFilter(const INavMeshQueryFilter& filter, const CNavMesh& navMesh, float minIslandArea)
		: filter(filter)
		, navMesh(navMesh)
		, minIslandArea(minIslandArea)
	{}

	bool PassFilter(const Tile::STriangle& triangle) const
	{
		return filter.PassFilter(triangle) && (navMesh.GetIslands().GetIslandArea(triangle.islandID) >= minIslandArea);
	}

	const INavMeshQueryFilter& filter;
	const CNavMesh& navMesh;
	float           minIslandArea;
};

template<typename TFilter>
size_t CNavMesh::QueryTileTrianglesLinear(const TileID tileID, const STile& tile, const aabb_t& queryAabbTile, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const
{
	CRY_ASSERT(maxTrianglesCount > 0);
	if (maxTrianglesCount == 0)
		return 0;

	size_t triCount = 0;
	for (size_t i = 0; i < tile.triangleCount; ++i)
	{
		const Tile::STriangle& triangle = tile.triangles[i];

		const Tile::Vertex& v0 = tile.vertices[triangle.vertex[0]];
		const Tile::Vertex& v1 = tile.vertices[triangle.vertex[1]];
		const Tile::Vertex& v2 = tile.vertices[triangle.vertex[2]];

		const aabb_t triaabb(vector3_t::minimize(v0, v1, v2), vector3_t::maximize(v0, v1, v2));

		if (queryAabbTile.overlaps(triaabb))
		{
			const TriangleID triangleID = ComputeTriangleID(tileID, static_cast<uint16>(i));

			if (filter.PassFilter(triangle))
			{
				pOutTriangles[triCount++] = triangleID;

				if (triCount == maxTrianglesCount)
					break;
			}
		}
	}
	return triCount;
}

template<typename TFilter>
size_t CNavMesh::QueryTileTrianglesBV(const TileID tileID, const STile& tile, const aabb_t& queryAabbTile, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const
{
	CRY_ASSERT(maxTrianglesCount > 0);
	if (maxTrianglesCount == 0)
		return 0;

	size_t triCount = 0;

	const size_t nodeCount = tile.nodeCount;
	size_t nodeID = 0;
	while (nodeID < nodeCount)
	{
		const Tile::SBVNode& node = tile.nodes[nodeID];

		if (!queryAabbTile.overlaps(node.aabb))
			nodeID += node.leaf ? 1 : node.offset;
		else
		{
			++nodeID;

			if (node.leaf)
			{
				const uint16 triangleIdx = node.offset;
				const TriangleID triangleID = ComputeTriangleID(tileID, triangleIdx);
				const Tile::STriangle& triangle = GetTriangleUnsafe(tileID, triangleIdx);

				if (filter.PassFilter(triangle))
				{
					pOutTriangles[triCount++] = triangleID;

					if (triCount == maxTrianglesCount)
						break;
				}
			}
		}
	}
	return triCount;
}

template<typename TFilter>
size_t CNavMesh::QueryTileTriangles(const TileID tileID, const vector3_t& tileOrigin, const aabb_t& queryAabbWorld, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const
{
	const STile& tile = GetTile(tileID);

	const vector3_t tileMin(0, 0, 0);
	const vector3_t tileMax(m_params.tileSize.x, m_params.tileSize.y, m_params.tileSize.z);

	aabb_t queryAabbTile(queryAabbWorld);
	queryAabbTile.min = vector3_t::maximize(queryAabbTile.min - tileOrigin, tileMin);
	queryAabbTile.max = vector3_t::minimize(queryAabbTile.max - tileOrigin, tileMax);

	if (!tile.nodeCount)
	{
		return QueryTileTrianglesLinear(tileID, tile, queryAabbTile, filter, maxTrianglesCount, pOutTriangles);
	}
	else
	{
		return QueryTileTrianglesBV(tileID, tile, queryAabbTile, filter, maxTrianglesCount, pOutTriangles);
	}
}

template<typename TFilter>
size_t CNavMesh::QueryTrianglesWithFilterInternal(const aabb_t& queryAabbWorld, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const
{
	CRY_ASSERT(pOutTriangles);
	CRY_ASSERT(maxTrianglesCount > 0);
	if (!(pOutTriangles && maxTrianglesCount > 0))
	{
		return 0;
	}

	const size_t minX = (std::max(queryAabbWorld.min.x, real_t(0)) / real_t(m_params.tileSize.x)).as_uint();
	const size_t minY = (std::max(queryAabbWorld.min.y, real_t(0)) / real_t(m_params.tileSize.y)).as_uint();
	const size_t minZ = (std::max(queryAabbWorld.min.z, real_t(0)) / real_t(m_params.tileSize.z)).as_uint();

	const size_t maxX = (std::max(queryAabbWorld.max.x, real_t(0)) / real_t(m_params.tileSize.x)).as_uint();
	const size_t maxY = (std::max(queryAabbWorld.max.y, real_t(0)) / real_t(m_params.tileSize.y)).as_uint();
	const size_t maxZ = (std::max(queryAabbWorld.max.z, real_t(0)) / real_t(m_params.tileSize.z)).as_uint();

	size_t triCount = 0;

	TriangleID* pTrianglesBegin = pOutTriangles;
	size_t maxTrianglesCountLeft = maxTrianglesCount;

	for (uint y = minY; y <= maxY; ++y)
	{
		for (uint x = minX; x <= maxX; ++x)
		{
			for (uint z = minZ; z <= maxZ; ++z)
			{
				if (const TileID tileID = GetTileID(x, y, z))
				{
					const vector3_t tileOrigin(
					  real_t(x * m_params.tileSize.x),
					  real_t(y * m_params.tileSize.y),
					  real_t(z * m_params.tileSize.z));

					const size_t trianglesInTileCount = QueryTileTriangles(tileID, tileOrigin, queryAabbWorld, filter, maxTrianglesCountLeft, pTrianglesBegin);

					CRY_ASSERT(maxTrianglesCountLeft >= trianglesInTileCount);
					maxTrianglesCountLeft -= trianglesInTileCount;
					pTrianglesBegin += trianglesInTileCount;
					triCount += trianglesInTileCount;

					if (maxTrianglesCountLeft == 0)
					{
						return triCount;
					}
				}
			}
		}
	}

	return triCount;
}

TriangleID CNavMesh::FindClosestTriangleInternal(
  const vector3_t& queryPosWorld,
  const TriangleID* pCandidateTriangles,
  const size_t candidateTrianglesCount,
  vector3_t* pOutClosestPosWorld,
  real_t::unsigned_overflow_type* pOutClosestDistanceSq) const
{
	TriangleID closestID = Constants::InvalidTriangleID;
	MNM::real_t::unsigned_overflow_type closestDistanceSq = std::numeric_limits<MNM::real_t::unsigned_overflow_type>::max();
	vector3_t closestPos(real_t::max());

	if (candidateTrianglesCount)
	{
		MNM::vector3_t a, b, c;

		for (size_t i = 0; i < candidateTrianglesCount; ++i)
		{
			const TriangleID triangleId = pCandidateTriangles[i];

			if (GetVertices(triangleId, a, b, c))
			{
				const MNM::vector3_t ptClosest = ClosestPtPointTriangle(queryPosWorld, a, b, c);
				const MNM::real_t::unsigned_overflow_type dSq = (ptClosest - queryPosWorld).lenSqNoOverflow();

				if (dSq < closestDistanceSq)
				{
					closestID = triangleId;
					closestDistanceSq = dSq;
					closestPos = ptClosest;
				}
			}
		}
	}

	if (closestID != Constants::InvalidTriangleID)
	{
		if (pOutClosestPosWorld)
		{
			*pOutClosestPosWorld = closestPos;
		}

		if (pOutClosestDistanceSq)
		{
			*pOutClosestDistanceSq = closestDistanceSq;
		}
	}

	return closestID;
}

#pragma warning(push)
#pragma warning(disable:28285)
size_t CNavMesh::GetTriangles(aabb_t aabb, TriangleID* triangles, size_t maxTriCount, const INavMeshQueryFilter* pFilter, float minIslandArea) const
{
	if (minIslandArea <= 0.0f)
	{
		if (pFilter)
		{
			return QueryTrianglesWithFilterInternal(aabb, *pFilter, maxTriCount, triangles);
		}
		else
		{
			SAcceptAllQueryTrianglesFilter filter;
			return QueryTrianglesWithFilterInternal(aabb, filter, maxTriCount, triangles);
		}
	}
	else
	{
		if (pFilter)
		{
			SNavigationQueryFilterWithMinIslandFilter filter(*pFilter, *this, minIslandArea);
			return QueryTrianglesWithFilterInternal(aabb, filter, maxTriCount, triangles);
		}
		else
		{
			SMinIslandAreaQueryTrianglesFilter filter(*this, minIslandArea);
			return QueryTrianglesWithFilterInternal(aabb, filter, maxTriCount, triangles);
		}
	}
}
#pragma warning(pop)

TriangleID CNavMesh::GetTriangleAt(const vector3_t& location, const real_t verticalDownwardRange, const real_t verticalUpwardRange, const INavMeshQueryFilter* pFilter, float minIslandArea /*= 0.0f*/) const
{
	const MNM::aabb_t aabb(
	  MNM::vector3_t(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z - verticalDownwardRange)),
	  MNM::vector3_t(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z + verticalUpwardRange)));

	const size_t MaxTriCandidateCount = 1024;
	TriangleID candidates[MaxTriCandidateCount];

	TriangleID closestID = 0;

	const size_t candidateCount = GetTriangles(aabb, candidates, MaxTriCandidateCount, pFilter, minIslandArea);
	MNM::real_t::unsigned_overflow_type distMinSq = std::numeric_limits<MNM::real_t::unsigned_overflow_type>::max();

	if (candidateCount)
	{
		MNM::vector3_t a, b, c;

		for (size_t i = 0; i < candidateCount; ++i)
		{
			GetVertices(candidates[i], a, b, c);

			if (PointInTriangle(vector2_t(location), vector2_t(a), vector2_t(b), vector2_t(c)))
			{
				const MNM::vector3_t ptClosest = ClosestPtPointTriangle(location, a, b, c);
				const MNM::real_t::unsigned_overflow_type dSq = (ptClosest - location).lenSqNoOverflow();

				if (dSq < distMinSq)
				{
					distMinSq = dSq;
					closestID = candidates[i];
				}
			}
		}
	}
	return closestID;
}

bool CNavMesh::IsTriangleAcceptableForLocation(const vector3_t& location, TriangleID triangleID) const
{
	//TODO: Use filter?

	const MNM::real_t range = MNM::real_t(1.0f);
	if (triangleID)
	{
		const MNM::aabb_t aabb(
		  MNM::vector3_t(MNM::real_t(location.x - range), MNM::real_t(location.y - range), MNM::real_t(location.z - range)),
		  MNM::vector3_t(MNM::real_t(location.x + range), MNM::real_t(location.y + range), MNM::real_t(location.z + range)));

		const size_t MaxTriCandidateCount = 1024;
		TriangleID candidates[MaxTriCandidateCount];

		const size_t candidateCount = GetTriangles(aabb, candidates, MaxTriCandidateCount, nullptr);
		MNM::real_t distMinSq = MNM::real_t::max();

		if (candidateCount)
		{
			MNM::vector3_t start = location;
			MNM::vector3_t a, b, c;

			for (size_t i = 0; i < candidateCount; ++i)
			{
				GetVertices(candidates[i], a, b, c);

				if (candidates[i] == triangleID && PointInTriangle(vector2_t(location), vector2_t(a), vector2_t(b), vector2_t(c)))
				{
					return true;
				}
			}
		}
	}

	return false;
}

TriangleID CNavMesh::GetClosestTriangle(const vector3_t& location, real_t vrange, real_t hrange, const INavMeshQueryFilter* pFilter, real_t* distance,
                                        vector3_t* closest, float minIslandArea) const
{
	const MNM::aabb_t aabb(
	  MNM::vector3_t(MNM::real_t(location.x - hrange), MNM::real_t(location.y - hrange), MNM::real_t(location.z - vrange)),
	  MNM::vector3_t(MNM::real_t(location.x + hrange), MNM::real_t(location.y + hrange), MNM::real_t(location.z + vrange)));

	const size_t MaxTriCandidateCount = 1024;
	TriangleID candidates[MaxTriCandidateCount];

	const size_t candidatesCount = GetTriangles(aabb, candidates, MaxTriCandidateCount, pFilter, minIslandArea);
	real_t::unsigned_overflow_type distanceSq;
	const TriangleID resultClosestTriangleId = FindClosestTriangleInternal(location, candidates, candidatesCount, closest, &distanceSq);

	if ((resultClosestTriangleId != Constants::InvalidTriangleID) && (distance != nullptr))
	{
		*distance = real_t::sqrtf(distanceSq);
	}

	return resultClosestTriangleId;
}

TriangleID CNavMesh::GetClosestTriangle(const vector3_t& location, const aabb_t& aroundLocationAABB, const INavMeshQueryFilter* pFilter,
	real_t* distance /*= nullptr*/, vector3_t* closest /*= nullptr*/, float minIslandArea /*= 0.0f*/) const
{
	const MNM::aabb_t aabb(location + aroundLocationAABB.min , location + aroundLocationAABB.max);

	const size_t MaxTriCandidateCount = 1024;
	TriangleID candidates[MaxTriCandidateCount];

	const size_t candidatesCount = GetTriangles(aabb, candidates, MaxTriCandidateCount, pFilter, minIslandArea);
	real_t::unsigned_overflow_type distanceSq;
	const TriangleID resultClosestTriangleId = FindClosestTriangleInternal(location, candidates, candidatesCount, closest, &distanceSq);

	if ((resultClosestTriangleId != Constants::InvalidTriangleID) && (distance != nullptr))
	{
		*distance = real_t::sqrtf(distanceSq);
	}

	return resultClosestTriangleId;
}


bool CNavMesh::GetVertices(TriangleID triangleID, vector3_t& v0, vector3_t& v1, vector3_t& v2) const
{
	if (const TileID tileID = ComputeTileID(triangleID))
	{
		const TileContainer& container = m_tiles[tileID - 1];

		const vector3_t origin(
		  real_t(container.x * m_params.tileSize.x),
		  real_t(container.y * m_params.tileSize.y),
		  real_t(container.z * m_params.tileSize.z));

		const Tile::STriangle& triangle = container.tile.triangles[ComputeTriangleIndex(triangleID)];
		v0 = origin + vector3_t(container.tile.vertices[triangle.vertex[0]]);
		v1 = origin + vector3_t(container.tile.vertices[triangle.vertex[1]]);
		v2 = origin + vector3_t(container.tile.vertices[triangle.vertex[2]]);

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

size_t CNavMesh::GetMeshBorders(const aabb_t& aabb, const INavMeshQueryFilter* pFilter, Vec3* pBorders, const size_t maxBorderCount, const float minIslandArea /* = 0.0f*/) const
{
	const size_t maxTriangleCount = 4096;
	TriangleID triangleIDs[maxTriangleCount];
	const size_t triangleCount = GetTriangles(aabb, triangleIDs, maxTriangleCount, pFilter, minIslandArea);

	if (pFilter)
	{
		return GetTrianglesBordersWithFilter(triangleIDs, triangleCount, *pFilter, pBorders, maxBorderCount);
	}
	else
	{
		return GetTrianglesBordersNoFilter(triangleIDs, triangleCount, pBorders, maxBorderCount);
	}
}

size_t CNavMesh::GetTrianglesBordersNoFilter(const TriangleID* triangleIDs, const size_t triangleCount, Vec3* pBorders, const size_t maxBorderCount) const
{
	size_t numBorders = 0;
	for (size_t i = 0; i < triangleCount; ++i)
	{
		const TileID tileID = ComputeTileID(triangleIDs[i]);
		uint16 triangleIdx = ComputeTriangleIndex(triangleIDs[i]);
		if (!tileID)
			continue;

		const TileContainer& container = m_tiles[tileID - 1];
		const Tile::STriangle& triangle = container.tile.triangles[triangleIdx];

		size_t linkedEdges = 0;
		for (size_t l = 0; l < triangle.linkCount; ++l)
		{
			const Tile::SLink& link = container.tile.links[triangle.firstLink + l];
			const size_t edge = link.edge;
			linkedEdges |= static_cast<size_t>(1) << edge;
		}

		if (linkedEdges == 7)
			continue;

		const vector3_t origin(real_t(container.x * m_params.tileSize.x), real_t(container.y * m_params.tileSize.y), real_t(container.z * m_params.tileSize.z));
		vector3_t verts[3];
		verts[0] = origin + vector3_t(container.tile.vertices[triangle.vertex[0]]);
		verts[1] = origin + vector3_t(container.tile.vertices[triangle.vertex[1]]);
		verts[2] = origin + vector3_t(container.tile.vertices[triangle.vertex[2]]);

		for (size_t e = 0; e < 3; ++e)
		{
			if ((linkedEdges & (size_t(1) << e)) == 0)
			{
				if (pBorders != NULL)
				{
					const Vec3 v0 = verts[e].GetVec3();
					const Vec3 v1 = verts[(e + 1) % 3].GetVec3();
					const Vec3 vOther = verts[(e + 2) % 3].GetVec3();

					const Vec3 edge = Vec3(v0 - v1).GetNormalized();
					const Vec3 otherEdge = Vec3(v0 - vOther).GetNormalized();
					const Vec3 up = edge.Cross(otherEdge);
					const Vec3 out = up.Cross(edge);

					pBorders[numBorders * 3 + 0] = v0;
					pBorders[numBorders * 3 + 1] = v1;
					pBorders[numBorders * 3 + 2] = out;
				}

				++numBorders;

				if (pBorders != NULL && numBorders == maxBorderCount)
					return numBorders;
			}
		}
	}
	return numBorders;
}

size_t CNavMesh::GetTrianglesBordersWithFilter(const TriangleID* triangleIDs, const size_t triangleCount, const INavMeshQueryFilter& filter, Vec3* pBorders, const size_t maxBorderCount) const
{
	size_t numBorders = 0;
	for (size_t i = 0; i < triangleCount; ++i)
	{
		const TileID tileID = ComputeTileID(triangleIDs[i]);
		uint16 triangleIdx = ComputeTriangleIndex(triangleIDs[i]);
		if (!tileID)
			continue;

		const TileContainer& container = m_tiles[tileID - 1];
		const Tile::STriangle& triangle = container.tile.triangles[triangleIdx];

		size_t linkedEdges = 0;
		for (size_t l = 0; l < triangle.linkCount; ++l)
		{
			const Tile::SLink& link = container.tile.links[triangle.firstLink + l];
			const size_t edge = link.edge;

			if (link.side == Tile::SLink::Internal)
			{
				Tile::STriangle& neighbourTriangle = GetTriangleUnsafe(tileID, link.triangle);
				if (filter.PassFilter(neighbourTriangle))
				{
					linkedEdges |= static_cast<size_t>(1) << edge;
				}
			}
			else if (link.side != Tile::SLink::OffMesh)
			{
				TileID neighbourTileID = GetNeighbourTileID(container.x, container.y, container.z, link.side);
				Tile::STriangle& neighbourTriangle = GetTriangleUnsafe(neighbourTileID, link.triangle);
				if (filter.PassFilter(neighbourTriangle))
				{
					linkedEdges |= static_cast<size_t>(1) << edge;
				}
			}
		}

		if (linkedEdges == 7)
			continue;

		const vector3_t origin(real_t(container.x * m_params.tileSize.x), real_t(container.y * m_params.tileSize.y), real_t(container.z * m_params.tileSize.z));
		vector3_t verts[3];
		verts[0] = origin + vector3_t(container.tile.vertices[triangle.vertex[0]]);
		verts[1] = origin + vector3_t(container.tile.vertices[triangle.vertex[1]]);
		verts[2] = origin + vector3_t(container.tile.vertices[triangle.vertex[2]]);

		for (size_t e = 0; e < 3; ++e)
		{
			if ((linkedEdges & (size_t(1) << e)) == 0)
			{
				if (pBorders != NULL)
				{
					const Vec3 v0 = verts[e].GetVec3();
					const Vec3 v1 = verts[(e + 1) % 3].GetVec3();
					const Vec3 vOther = verts[(e + 2) % 3].GetVec3();

					const Vec3 edge = Vec3(v0 - v1).GetNormalized();
					const Vec3 otherEdge = Vec3(v0 - vOther).GetNormalized();
					const Vec3 up = edge.Cross(otherEdge);
					const Vec3 out = up.Cross(edge);

					pBorders[numBorders * 3 + 0] = v0;
					pBorders[numBorders * 3 + 1] = v1;
					pBorders[numBorders * 3 + 2] = out;
				}

				++numBorders;

				if (pBorders != NULL && numBorders == maxBorderCount)
					return numBorders;
			}
		}
	}
	return numBorders;
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

void CNavMesh::SetTrianglesAnnotation(const MNM::TriangleID* pTrianglesArray, const size_t trianglesCount, const MNM::AreaAnnotation areaAnnotation, std::vector<TileID>& affectedTiles)
{
	for (size_t i = 0; i < trianglesCount; ++i)
	{
		const MNM::TriangleID triangleId = pTrianglesArray[i];
		if (const MNM::TileID tileId = MNM::ComputeTileID(triangleId))
		{
			const uint16 triangleIndex = MNM::ComputeTriangleIndex(triangleId);
			MNM::Tile::STriangle& triangle = GetTriangleUnsafe(tileId, triangleIndex);
			if (triangle.areaAnnotation == areaAnnotation)
				continue;

			triangle.areaAnnotation = areaAnnotation;

			stl::push_back_unique(affectedTiles, MNM::ComputeTileID(triangleId));
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

bool CNavMesh::SnapPosition(const vector3_t& position, const aabb_t& aroundPositionAABB, const SSnapToNavMeshRulesInfo& snappingRules, const INavMeshQueryFilter* pFilter, vector3_t& snappedPosition, MNM::TriangleID* pTriangleId) const
{
	// Note: GetClosestTriangle and GetTriangleAt are expecting input positions in navmesh's local space, but returned snapped position is in global space
	// We'll keep the same behavior here for now
	
	if (snappingRules.bVerticalSearch)
	{
		if (TriangleID triangleId = GetTriangleAt(position, -aroundPositionAABB.min.z, aroundPositionAABB.max.z, pFilter, 0.0f))
		{
			// TODO: get the position projected down on the triangle
			// Using input position in global space for now
			snappedPosition = position + vector3_t(m_params.origin);

			if (pTriangleId)
				*pTriangleId = triangleId;
			return true;
		}
	}
	if (snappingRules.bBoxSearch)
	{
		if (TriangleID triangleId = GetClosestTriangle(position, aroundPositionAABB, pFilter, nullptr, &snappedPosition))
		{
			if (pTriangleId)
				*pTriangleId = triangleId;
			return true;
		}
	}
	return false;
}

bool CNavMesh::PushPointInsideTriangle(const TriangleID triangleID, vector3_t& location, real_t amount) const
{
	if (amount <= 0)
		return false;

	vector3_t v0, v1, v2;
	if (const TileID tileID = ComputeTileID(triangleID))
	{
		const TileContainer& container = m_tiles[tileID - 1];
		const vector3_t origin(
		  real_t(container.x * m_params.tileSize.x),
		  real_t(container.y * m_params.tileSize.y),
		  real_t(container.z * m_params.tileSize.z));
		const vector3_t locationTileOffsetted = (location - origin);

		const Tile::STriangle& triangle = container.tile.triangles[ComputeTriangleIndex(triangleID)];
		v0 = vector3_t(container.tile.vertices[triangle.vertex[0]]);
		v1 = vector3_t(container.tile.vertices[triangle.vertex[1]]);
		v2 = vector3_t(container.tile.vertices[triangle.vertex[2]]);

		const vector3_t triangleCenter = ((v0 + v1 + v2) * real_t::fraction(1, 3));
		const vector3_t locationToCenter = triangleCenter - locationTileOffsetted;
		const real_t locationToCenterLen = locationToCenter.lenNoOverflow();

		if (locationToCenterLen > amount)
		{
			location += (locationToCenter / locationToCenterLen) * amount;
		}
		else
		{
			// If locationToCenterLen is smaller than the amount I wanna push
			// the point, then it's safer use directly the center position
			// otherwise the point could end up outside the other side of the triangle
			location = triangleCenter + origin;
		}
		return true;
	}

	return false;
}

void CNavMesh::PredictNextTriangleEntryPosition(const TriangleID bestNodeTriangleID,
                                                const vector3_t& bestNodePosition, const TriangleID nextTriangleID, const unsigned int edgeIndex
                                                , const vector3_t& finalLocation, vector3_t& outPosition) const
{
	IF_UNLIKELY (edgeIndex == MNM::Constants::InvalidEdgeIndex)
	{
		// For SO links we don't set up the edgeIndex value since it's more probable that the animations
		// ending point is better approximated by the triangle center value
		vector3_t v0, v1, v2;
		GetVertices(nextTriangleID, v0, v1, v2);
		outPosition = (v0 + v1 + v2) * real_t::fraction(1, 3);
		return;
	}

	Tile::STriangle triangle;
	if (GetTriangle(bestNodeTriangleID, triangle))
	{
		const TileID bestTriangleTileID = ComputeTileID(bestNodeTriangleID);
		assert(bestTriangleTileID);
		const TileContainer& currentContainer = m_tiles[bestTriangleTileID - 1];
		const STile& currentTile = currentContainer.tile;
		const vector3_t tileOrigin(real_t(currentContainer.x * m_params.tileSize.x), real_t(currentContainer.y * m_params.tileSize.y), real_t(currentContainer.z * m_params.tileSize.z));

		assert(edgeIndex < 3);
		const vector3_t v0 = tileOrigin + vector3_t(currentTile.vertices[triangle.vertex[edgeIndex]]);
		const vector3_t v1 = tileOrigin + vector3_t(currentTile.vertices[triangle.vertex[next_mod3(edgeIndex)]]);

		switch (gAIEnv.CVars.MNMPathfinderPositionInTrianglePredictionType)
		{
		case ePredictionType_TriangleCenter:
			{
				const vector3_t v2 = tileOrigin + vector3_t(currentTile.vertices[triangle.vertex[dec_mod3[edgeIndex]]]);
				outPosition = (v0 + v1 + v2) * real_t::fraction(1, 3);
			}
			break;
		case ePredictionType_Advanced:
		default:
			{
				const vector3_t v0v1 = v1 - v0;
				real_t s, t;
				if (IntersectSegmentSegment(v0, v1, bestNodePosition, finalLocation, s, t))
				{
					// If the two segments intersect,
					// let's choose the point that goes in the direction we want to go
					s = clamp(s, kMinPullingThreshold, kMaxPullingThreshold);
					outPosition = v0 + v0v1 * s;
				}
				else
				{
					// Otherwise we need to understand where the segment is in relation
					// of where we want to go.
					// Let's choose the point on the segment that is closer towards the target
					const real_t::unsigned_overflow_type distSqAE = (v0 - finalLocation).lenSqNoOverflow();
					const real_t::unsigned_overflow_type distSqBE = (v1 - finalLocation).lenSqNoOverflow();
					const real_t segmentPercentage = (distSqAE < distSqBE) ? kMinPullingThreshold : kMaxPullingThreshold;
					outPosition = v0 + v0v1 * segmentPercentage;
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

CNavMesh::EWayQueryResult CNavMesh::FindWay(WayQueryRequest& inputRequest, WayQueryWorkingSet& workingSet, WayQueryResult& result) const
{
	if (inputRequest.GetFilter())
	{
		return FindWayInternal(inputRequest, workingSet, *inputRequest.GetFilter(), result);
	}
	else
	{
		SAcceptAllQueryTrianglesFilter filter;
		return FindWayInternal(inputRequest, workingSet, filter, result);
	}
}

template<typename TFilter>
CNavMesh::EWayQueryResult CNavMesh::FindWayInternal(WayQueryRequest& inputRequest, WayQueryWorkingSet& workingSet, const TFilter& filter, WayQueryResult& result) const
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
			pOutputWay[0] = WayTriangleData(inputRequest.From(), 0);
			pOutputWay[1] = WayTriangleData(inputRequest.To(), 0);
			result.SetWaySize(2);
			return eWQR_Done;
		}
		else
		{
			const vector3_t origin(m_params.origin);
			const vector3_t startLocation = inputRequest.GetFromLocation();
			const vector3_t endLocation = inputRequest.GetToLocation();

			WayTriangleData lastBestNodeID(inputRequest.From(), 0);

			while (workingSet.aStarOpenList.CanDoStep())
			{
				// switch the smallest element with the last one and pop the last element
				AStarOpenList::OpenNodeListElement element = workingSet.aStarOpenList.PopBestNode();
				WayTriangleData bestNodeID = element.triData;

				lastBestNodeID = bestNodeID;
				IF_UNLIKELY (bestNodeID.triangleID == inputRequest.To())
				{
					workingSet.aStarOpenList.StepDone();
					break;
				}

				AStarOpenList::Node* bestNode = element.pNode;
				const vector3_t bestNodeLocation = bestNode->location - origin;

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

						WayTriangleData nextTri(0, 0);

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
							//TODO: apply filter also to offmesh links
							OffMeshNavigation::QueryLinksResult links = inputRequest.GetOffMeshNavigation().GetLinksForTriangle(bestNodeID.triangleID, link.triangle);
							while (nextTri = links.GetNextTriangle())
							{
								if (inputRequest.CanUseOffMeshLink(nextTri.offMeshLinkID, &nextTri.costMultiplier))
								{
									workingSet.nextLinkedTriangles.push_back(nextTri);
									nextTri.incidentEdge = (unsigned int)MNM::Constants::InvalidEdgeIndex;
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

					AStarOpenList::Node* nextNode = NULL;
					const bool inserted = workingSet.aStarOpenList.InsertNode(nextTri, &nextNode);

					assert(nextNode);

					if (inserted)
					{
						IF_UNLIKELY (nextTri.triangleID == inputRequest.To())
						{
							nextNode->location = endLocation;
						}
						else
						{
							PredictNextTriangleEntryPosition(bestNodeID.triangleID, bestNodeLocation, nextTri.triangleID, nextTri.incidentEdge, endLocation, nextNode->location);
						}

						nextNode->open = false;
					}

					//Euclidean distance
					const vector3_t targetDistance = endLocation - nextNode->location;
					const vector3_t stepDistance = bestNodeLocation - nextNode->location;
					const real_t heuristic = targetDistance.lenNoOverflow();
					const real_t stepCost = stepDistance.lenNoOverflow();

					//Euclidean distance approximation
					//const real_t heuristic = targetDistance.approximatedLen();
					//const real_t stepCost = stepDistance.approximatedLen();

					const real_t dangersTotalCost = CalculateHeuristicCostForDangers(nextNode->location, startLocation, m_params.origin, inputRequest.GetDangersInfos());
					const real_t customCost = CalculateHeuristicCostForCustomRules(bestNode->location, nextNode->location, m_params.origin, inputRequest.GetCustomPathCostComputer().get());

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
						nextNode->location += origin;
						workingSet.aStarOpenList.AddToOpenList(nextTri, nextNode, total);
					}
				}

				workingSet.aStarOpenList.StepDone();
			}

			if (lastBestNodeID.triangleID == inputRequest.To())
			{
				size_t wayTriCount = 0;
				WayTriangleData wayTriangle = lastBestNodeID;
				WayTriangleData nextInsertion(wayTriangle.triangleID, 0);

				WayTriangleData* outputWay = result.GetWayData();

				while (wayTriangle.triangleID != inputRequest.From())
				{
					const AStarOpenList::Node* node = workingSet.aStarOpenList.FindNode(wayTriangle);
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
			else if (!workingSet.aStarOpenList.Empty())
			{
				//We did not finish yet...
				return eWQR_Continuing;
			}
		}
	}

	return eWQR_Done;
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
	real_t&     m_totalCost;
	const Vec3& m_locationToEvaluate;
	const Vec3& m_startingLocation;
};

real_t CNavMesh::CalculateHeuristicCostForDangers(const vector3_t& locationToEval, const vector3_t& startingLocation, const Vec3& meshOrigin, const DangerousAreasList& dangersInfos) const
{
	real_t totalCost(0.0f);
	const Vec3 startingLocationInWorldSpace = startingLocation.GetVec3() + meshOrigin;
	const Vec3 locationInWorldSpace = locationToEval.GetVec3() + meshOrigin;
	std::for_each(dangersInfos.begin(), dangersInfos.end(), CostAccumulator(locationInWorldSpace, startingLocationInWorldSpace, totalCost));
	return totalCost;
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

void CNavMesh::PullString(const vector3_t& from, const TriangleID fromTriID, const vector3_t& to, const TriangleID toTriID, vector3_t& middlePoint) const
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
					vi1 = next_mod3(link.edge);
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
						vi1 = next_mod3(link.edge);
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
			if (!IntersectSegmentSegment(vector2_t(fromVertices[vi0]),
			                             vector2_t(fromVertices[vi1]), vector2_t(from), vector2_t(to), s, t))
			{
				s = 0;
			}

			// Even if segments don't intersect, s = 0 and is clamped to the pulling threshold range
			s = clamp(s, kMinPullingThreshold, kMaxPullingThreshold);

			middlePoint = Lerp(fromVertices[vi0], fromVertices[vi1], s);
		}
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

inline size_t OppositeSide(size_t side)
{
	return (side + 7) % 14;
}

CNavMesh::ERayCastResult CNavMesh::RayCast(const vector3_t& from, TriangleID fromTri, const vector3_t& to, TriangleID toTri,
                                           RaycastRequestBase& raycastRequest, const INavMeshQueryFilter* pFilter) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	switch (gAIEnv.CVars.MNMRaycastImplementation)
	{
	case 0:
		return RayCast_v1(from, fromTri, to, toTri, raycastRequest);
	case 1:
		return RayCast_v2(from, fromTri, to, toTri, raycastRequest);
	case 2:
	default:
	{
		// toTri parameter not used in this version
		if (pFilter)
		{
			return RayCast_v3(from, fromTri, to, *pFilter, raycastRequest);
		}
		else
		{
			SAcceptAllQueryTrianglesFilter filter;
			return RayCast_v3(from, fromTri, to, filter, raycastRequest);
		}
	}
	}
}

struct RaycastNode
{
	RaycastNode()
		: triangleID(MNM::Constants::InvalidTriangleID)
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

	ILINE bool IsValid() const { return triangleID != MNM::Constants::InvalidTriangleID; }

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

CNavMesh::ERayCastResult CNavMesh::RayCast_v2(const vector3_t& from, TriangleID fromTriangleID, const vector3_t& to, TriangleID toTriangleID,
	RaycastRequestBase& raycastRequest) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!IsLocationInTriangle(from, fromTriangleID))
		return eRayCastResult_InvalidStart;

#ifdef RAYCAST_DO_NOT_ACCEPT_INVALID_END
	if (!IsLocationInTriangle(to, toTriangleID))
		return eRayCastResult_InvalidEnd;
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
			return ConstructRaycastResult(eRayCastResult_NoHit, rayHit, toTriangleID, cameFrom, raycastRequest);
		}

		if (currentNode.percentageOfTotalDistance > furtherNodeVisited.percentageOfTotalDistance)
			furtherNodeVisited = currentNode;

		rayHit.triangleID = currentNode.triangleID;
		RaycastNode nextNode;

		closedList.insert(currentNode.triangleID);

		TileID tileID = ComputeTileID(currentNode.triangleID);
		if (tileID != MNM::Constants::InvalidTileID)
		{
			const TileContainer* container = &m_tiles[tileID - 1];
			const STile* tile = &container->tile;

			vector3_t tileOrigin(
				real_t(container->x * m_params.tileSize.x),
				real_t(container->y * m_params.tileSize.y),
				real_t(container->z * m_params.tileSize.z));

			const Tile::STriangle& triangle = tile->triangles[ComputeTriangleIndex(currentNode.triangleID)];
			for (uint16 edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
			{
				if (currentNode.incidentEdge == edgeIndex)
					continue;

				const vector3_t a = tileOrigin + vector3_t(tile->vertices[triangle.vertex[edgeIndex]]);
				const vector3_t b = tileOrigin + vector3_t(tile->vertices[triangle.vertex[inc_mod3[edgeIndex]]]);

				real_t s, t;
				if (IntersectSegmentSegment(vector2_t(from), vector2_t(to), vector2_t(a), vector2_t(b), s, t))
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
							const uint16 currentOppositeSide = static_cast<uint16>(OppositeSide(side));

							for (size_t reciprocalLinkIndex = 0; reciprocalLinkIndex < opposite.linkCount; ++reciprocalLinkIndex)
							{
								const Tile::SLink& reciprocal = neighbourContainer.tile.links[opposite.firstLink + reciprocalLinkIndex];
								if ((reciprocal.triangle == currentTriangleIndex) && (reciprocal.side == currentOppositeSide))
								{
									const TriangleID possibleNextID = ComputeTriangleID(neighbourTileID, link.triangle);

									if (closedList.find(possibleNextID) != closedList.end())
										break;

									const vector3_t neighbourTileOrigin = vector3_t(
										real_t(neighbourContainer.x * m_params.tileSize.x),
										real_t(neighbourContainer.y * m_params.tileSize.y),
										real_t(neighbourContainer.z * m_params.tileSize.z));

									const uint16 i0 = reciprocal.edge;
									const uint16 i1 = inc_mod3[reciprocal.edge];

									assert(i0 < 3);
									assert(i1 < 3);

									const vector3_t c = neighbourTileOrigin + vector3_t(neighbourContainer.tile.vertices[opposite.vertex[i0]]);
									const vector3_t d = neighbourTileOrigin + vector3_t(neighbourContainer.tile.vertices[opposite.vertex[i1]]);

									real_t p, q;
									if (IntersectSegmentSegment(vector2_t(from), vector2_t(to), vector2_t(c), vector2_t(d), p, q))
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

	return ConstructRaycastResult(eRayCastResult_Hit, rayHit, furtherNodeVisited.triangleID, cameFrom, raycastRequest);
}

template<typename TFilter>
CNavMesh::ERayCastResult CNavMesh::RayCast_v3(const vector3_t& fromPos, TriangleID fromTriangleID, const vector3_t& toPos, const TFilter& filter, RaycastRequestBase& raycastRequest) const
{
	//TODO: take area costs into account and return total cost to traverse the ray
	
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!IsLocationInTriangle(fromPos, fromTriangleID))
		return eRayCastResult_InvalidStart;

	RaycastCameFromMap cameFrom;
	cameFrom.reserve(raycastRequest.maxWayTriCount);

	RayHit rayHit;
	rayHit.distance = 0.0f;
	rayHit.edge = MNM::Constants::InvalidEdgeIndex;
	rayHit.triangleID = MNM::Constants::InvalidTriangleID;

	TriangleID currentTriangleID = fromTriangleID;
	
	while (currentTriangleID != MNM::Constants::InvalidTriangleID)
	{
		TileID currentTileID = ComputeTileID(currentTriangleID);
		CRY_ASSERT(currentTileID != MNM::Constants::InvalidTileID);

		if (currentTileID == MNM::Constants::InvalidTileID)
			break;
		
		const TileContainer& currentContainer = m_tiles[currentTileID - 1];
		const STile& currentTile = currentContainer.tile;

		const uint16 currentTriangleIndex = ComputeTriangleIndex(currentTriangleID);
		const Tile::STriangle& currentTriangle = currentTile.triangles[currentTriangleIndex];

		const vector3_t currentTileOrigin(
			real_t(currentContainer.x * m_params.tileSize.x),
			real_t(currentContainer.y * m_params.tileSize.y),
			real_t(currentContainer.z * m_params.tileSize.z));

		//collect vertices of the current triangle
		vector2_t currentTriangleVertices[3];
		currentTriangleVertices[0] = vector2_t(currentTileOrigin + vector3_t(currentContainer.tile.vertices[currentTriangle.vertex[0]]));
		currentTriangleVertices[1] = vector2_t(currentTileOrigin + vector3_t(currentContainer.tile.vertices[currentTriangle.vertex[1]]));
		currentTriangleVertices[2] = vector2_t(currentTileOrigin + vector3_t(currentContainer.tile.vertices[currentTriangle.vertex[2]]));
		
		real_t rayIntersectionParam;
		uint16 intersectionEdgeIndex;
		const bool bEndingInside = FindNextIntersectingTriangleEdge(fromPos, toPos, currentTriangleVertices, rayIntersectionParam, intersectionEdgeIndex);

		if (intersectionEdgeIndex == uint16(MNM::Constants::InvalidEdgeIndex))
		{
			if (bEndingInside)
			{
				// Ray segment is ending in current triangle, return no hit
				rayHit.triangleID = currentTriangleID;
				rayHit.edge = intersectionEdgeIndex;
				rayHit.distance = 1.0f;
				return ConstructRaycastResult(eRayCastResult_NoHit, rayHit, rayHit.triangleID, cameFrom, raycastRequest);
			}
			else
			{
				// Ray segment missed the triangle, return the last hit
				return ConstructRaycastResult(eRayCastResult_Hit, rayHit, rayHit.triangleID, cameFrom, raycastRequest);
			}
		}

		rayHit.triangleID = currentTriangleID;
		rayHit.edge = intersectionEdgeIndex;
		if (rayIntersectionParam > rayHit.distance)
		{
			rayHit.distance = rayIntersectionParam;
		}
		
		TriangleID neighbourTriangleID = StepOverEdgeToNeighbourTriangle(fromPos, toPos, currentTileID, currentTriangleID, intersectionEdgeIndex, filter);
		if (neighbourTriangleID != MNM::Constants::InvalidTriangleID)
		{
			std::pair<RaycastCameFromMap::iterator, bool> insertResult = cameFrom.insert({ neighbourTriangleID, currentTriangleID });
			if (!insertResult.second)
			{
				// Triangle was already visited, we have a loop
				// This shouldn't happen in normal circumstances and it can mean that we have e.g. degenerate triangle
				neighbourTriangleID = MNM::Constants::InvalidTriangleID;
			}
		}
		currentTriangleID = neighbourTriangleID;
	}

	return ConstructRaycastResult(eRayCastResult_Hit, rayHit, rayHit.triangleID, cameFrom, raycastRequest);
}

template<typename TFilter>
MNM::TriangleID CNavMesh::StepOverEdgeToNeighbourTriangle(const vector3_t& rayStart, const vector3_t& rayEnd, const TileID currentTileID, const TriangleID currentTriangleID, const uint16 edgeIndex, const TFilter& filter) const
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
			Tile::STriangle& neighbourTriangle = GetTriangleUnsafe(currentTileID, link.triangle);
			return filter.PassFilter(neighbourTriangle) ? ComputeTriangleID(currentTileID, link.triangle) : MNM::Constants::InvalidTriangleID;
		}

		// Edge is on the tile boundaries, there can be more neighbour triangles adjacent to this edge
		TileID neighbourTileID = GetNeighbourTileID(currentContainer.x, currentContainer.y, currentContainer.z, link.side);
		const TileContainer& neighbourContainer = m_tiles[neighbourTileID - 1];
		const Tile::STriangle& neighbourTriangle = neighbourContainer.tile.triangles[link.triangle];
		
		if(!filter.PassFilter(neighbourTriangle))
			continue;

		const uint16 currentOppositeSide = static_cast<uint16>(OppositeSide(side));

		for (size_t reciprocalLinkIndex = 0; reciprocalLinkIndex < neighbourTriangle.linkCount; ++reciprocalLinkIndex)
		{
			const Tile::SLink& reciprocal = neighbourContainer.tile.links[neighbourTriangle.firstLink + reciprocalLinkIndex];
			if ((reciprocal.triangle == currentTriangleIndex) && (reciprocal.side == currentOppositeSide))
			{
				const vector3_t neighbourTileOrigin = vector3_t(
					real_t(neighbourContainer.x * m_params.tileSize.x),
					real_t(neighbourContainer.y * m_params.tileSize.y),
					real_t(neighbourContainer.z * m_params.tileSize.z));

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
				if (IntersectSegmentSegment(vector2_t(rayStart), vector2_t(rayEnd), vector2_t(edgeStart), vector2_t(edgeEnd), rayParameter, edgeParameter))
				{
					return ComputeTriangleID(neighbourTileID, link.triangle);
				}
			}
		}
	}

	return MNM::Constants::InvalidTriangleID;
}

CNavMesh::ERayCastResult CNavMesh::ConstructRaycastResult(const ERayCastResult returnResult, const RayHit& rayHit, const TriangleID lastTriangleID, const RaycastCameFromMap& cameFromMap,
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
			raycastRequest.result = eRayCastResult_RayTooLong;
			return eRayCastResult_RayTooLong;
		}

		raycastRequest.way[elementIndex++] = currentTriangleID;
		currentTriangleID = elementIterator->second;
		elementIterator = cameFromMap.find(currentTriangleID);
	}

	raycastRequest.way[elementIndex++] = currentTriangleID;
	raycastRequest.wayTriCount = elementIndex;
	raycastRequest.result = elementIndex < raycastRequest.maxWayTriCount ? returnResult : eRayCastResult_RayTooLong;

	return returnResult;
}

bool CNavMesh::IsLocationInTriangle(const vector3_t& location, const TriangleID triangleID) const
{
	if (triangleID == MNM::Constants::InvalidTriangleID)
		return false;

	TileID tileID = ComputeTileID(triangleID);
	if (tileID == MNM::Constants::InvalidTileID)
		return false;

	const TileContainer* container = &m_tiles[tileID - 1];
	const STile* tile = &container->tile;

	vector3_t tileOrigin(
	  real_t(container->x * m_params.tileSize.x),
	  real_t(container->y * m_params.tileSize.y),
	  real_t(container->z * m_params.tileSize.z));

	if (triangleID)
	{
		const Tile::STriangle& triangle = tile->triangles[ComputeTriangleIndex(triangleID)];

		const vector2_t a = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[0]]);
		const vector2_t b = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[1]]);
		const vector2_t c = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[2]]);

		return PointInTriangle(vector2_t(location), a, b, c);
	}

	return false;
}

CNavMesh::ERayCastResult CNavMesh::RayCast_v1(const vector3_t& from, TriangleID fromTri, const vector3_t& to, TriangleID toTri, RaycastRequestBase& raycastRequest) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (TileID tileID = ComputeTileID(fromTri))
	{
		const TileContainer* container = &m_tiles[tileID - 1];
		const STile* tile = &container->tile;

		vector3_t tileOrigin(
		  real_t(container->x * m_params.tileSize.x),
		  real_t(container->y * m_params.tileSize.y),
		  real_t(container->z * m_params.tileSize.z));

		if (fromTri)
		{
			const Tile::STriangle& triangle = tile->triangles[ComputeTriangleIndex(fromTri)];

			const vector2_t a = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[0]]);
			const vector2_t b = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[1]]);
			const vector2_t c = vector2_t(tileOrigin) + vector2_t(tile->vertices[triangle.vertex[2]]);

			if (!PointInTriangle(vector2_t(from), a, b, c))
				fromTri = 0;
		}

		if (!fromTri)
		{
			RayHit& hit = raycastRequest.hit;
			hit.distance = -real_t::max();
			hit.triangleID = 0;
			hit.edge = 0;

			return eRayCastResult_Hit;
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
				return eRayCastResult_RayTooLong;
			}

			if (toTri && currentID == toTri)
			{
				raycastRequest.wayTriCount = triCount;
				return eRayCastResult_NoHit;
			}

			const Tile::STriangle& triangle = tile->triangles[ComputeTriangleIndex(currentID)];
			TriangleID nextID = 0;
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
				const vector3_t b = tileOrigin + vector3_t(tile->vertices[triangle.vertex[next_mod3(e)]]);

				real_t s, t;
				if (IntersectSegmentSegment(vector2_t(from), vector2_t(to), vector2_t(a), vector2_t(b), s, t))
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
							const uint16 currentOppositeSide = static_cast<uint16>(OppositeSide(side));

							for (size_t rl = 0; rl < opposite.linkCount; ++rl)
							{
								const Tile::SLink& reciprocal = neighbourContainer.tile.links[opposite.firstLink + rl];
								if ((reciprocal.triangle == currentTriangleIndex) && (reciprocal.side == currentOppositeSide))
								{
									const vector3_t neighbourTileOrigin = vector3_t(
									  real_t(neighbourContainer.x * m_params.tileSize.x),
									  real_t(neighbourContainer.y * m_params.tileSize.y),
									  real_t(neighbourContainer.z * m_params.tileSize.z));

									const uint16 i0 = reciprocal.edge;
									const uint16 i1 = next_mod3(reciprocal.edge);

									assert(i0 < 3);
									assert(i1 < 3);

									const vector3_t c = neighbourTileOrigin + vector3_t(
									  neighbourContainer.tile.vertices[opposite.vertex[i0]]);
									const vector3_t d = neighbourTileOrigin + vector3_t(
									  neighbourContainer.tile.vertices[opposite.vertex[i1]]);

									const TriangleID possibleNextID = ComputeTriangleID(neighbourTileID, link.triangle);
									real_t p, q;
									if (IntersectSegmentSegment(vector2_t(from), vector2_t(to), vector2_t(c), vector2_t(d), p, q))
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
					if (IsTriangleAlreadyInWay(nextID, raycastRequest.way, triCount))
					{
						assert(0);
						nextID = 0;
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

							return eRayCastResult_Hit;
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

		bool isEndingTriangleAcceptable = IsTriangleAcceptableForLocation(to, raycastRequest.way[triCount - 1]);
		return isEndingTriangleAcceptable ? eRayCastResult_NoHit : eRayCastResult_Unacceptable;
	}

	return eRayCastResult_InvalidStart;
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

	std::pair<TileMap::iterator, bool> iresult = m_tileMap.insert(TileMap::value_type(tileName, 0));

	size_t tileID;

	if (iresult.second)
	{
		iresult.first->second = tileID = m_tiles.AllocateTileContainer();

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
			for (size_t side = 0; side < SideCount; ++side)
			{
				size_t nx = container.x + NavMesh::GetNeighbourTileOffset(side)[0];
				size_t ny = container.y + NavMesh::GetNeighbourTileOffset(side)[1];
				size_t nz = container.z + NavMesh::GetNeighbourTileOffset(side)[2];

				if (TileID neighbourID = GetTileID(nx, ny, nz))
				{
					TileContainer& ncontainer = m_tiles[neighbourID - 1];

					ReComputeAdjacency(ncontainer.x, ncontainer.y, ncontainer.z, kAdjecencyCalculationToleranceSq, ncontainer.tile,
					                   OppositeSide(side), container.x, container.y, container.z, tileID);
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

	for (; it != end; ++it)
	{
		const TileID tileID = it->second;

		TileContainer& container = m_tiles[tileID - 1];

		ComputeAdjacency(container.x, container.y, container.z, toleranceSq, container.tile);
	}
}

void CNavMesh::ConnectToNetwork(TileID tileID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	{
		TileContainer& container = m_tiles[tileID - 1];

		ComputeAdjacency(container.x, container.y, container.z, kAdjecencyCalculationToleranceSq, container.tile);

		for (size_t side = 0; side < SideCount; ++side)
		{
			size_t nx = container.x + NavMesh::GetNeighbourTileOffset(side)[0];
			size_t ny = container.y + NavMesh::GetNeighbourTileOffset(side)[1];
			size_t nz = container.z + NavMesh::GetNeighbourTileOffset(side)[2];

			if (TileID neighbourID = GetTileID(nx, ny, nz))
			{
				TileContainer& ncontainer = m_tiles[neighbourID - 1];

				ReComputeAdjacency(ncontainer.x, ncontainer.y, ncontainer.z, kAdjecencyCalculationToleranceSq, ncontainer.tile,
				                   OppositeSide(side), container.x, container.y, container.z, tileID);
			}
		}
	}
}

TileID CNavMesh::GetTileID(size_t x, size_t y, size_t z) const
{
	const size_t tileName = ComputeTileName(x, y, z);

	TileMap::const_iterator it = m_tileMap.find(tileName);
	if (it != m_tileMap.end())
		return it->second;
	return 0;
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
	const float maxDistanceToRenderSqr = sqr(gAIEnv.CVars.NavmeshTileDistanceDraw);

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

		vector3_t origin(
			real_t(m_params.origin.x + container.x * m_params.tileSize.x),
			real_t(m_params.origin.y + container.y * m_params.tileSize.y),
			real_t(m_params.origin.z + container.z * m_params.tileSize.z));

		Vec3 min = origin.GetVec3();
		if (cameraPos.GetSquaredDistance2D(min) < maxDistanceToRenderSqr && camera.IsAABBVisible_F(AABB(min, min + m_params.tileSize)))
		{
			container.tile.Draw(drawFlags, origin, it->second, islandAreas, colorSelector);
		}
	}
}

const CNavMesh::ProfilerType& CNavMesh::GetProfiler() const
{
	return m_profiler;
}

struct Edge
{
	uint16 vertex[2];
	uint16 triangle[2];
};

void ComputeTileTriangleAdjacency(const Tile::STriangle* triangles, const size_t triangleCount, const size_t vertexCount,
                                  Edge* edges, uint16* adjacency)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	{
		enum { Unused = 0xffff, };

		const size_t MaxLookUp = 4096;
		uint16 edgeLookUp[MaxLookUp];
		assert(MaxLookUp > vertexCount + triangleCount * 3);

		std::fill(&edgeLookUp[0], &edgeLookUp[0] + vertexCount, static_cast<uint16>(Unused));

		uint16 edgeCount = 0;

		for (uint16 i = 0; i < triangleCount; ++i)
		{
			const Tile::STriangle& triangle = triangles[i];

			for (size_t v = 0; v < 3; ++v)
			{
				uint16 i1 = triangle.vertex[v];
				uint16 i2 = triangle.vertex[next_mod3(v)];

				if (i1 < i2)
				{
					uint16 edgeIdx = edgeCount++;
					adjacency[i * 3 + v] = edgeIdx;

					Edge& edge = edges[edgeIdx];
					edge.triangle[0] = i;
					edge.triangle[1] = i;
					edge.vertex[0] = i1;
					edge.vertex[1] = i2;

					edgeLookUp[vertexCount + edgeIdx] = edgeLookUp[i1];
					edgeLookUp[i1] = edgeIdx;
				}
			}
		}

		for (uint16 i = 0; i < triangleCount; ++i)
		{
			const Tile::STriangle& triangle = triangles[i];

			for (size_t v = 0; v < 3; ++v)
			{
				uint16 i1 = triangle.vertex[v];
				uint16 i2 = triangle.vertex[next_mod3(v)];

				if (i1 > i2)
				{
					uint16 edgeIndex = edgeLookUp[i2];

					for (; edgeIndex != Unused; edgeIndex = edgeLookUp[vertexCount + edgeIndex])
					{
						Edge& edge = edges[edgeIndex];

						if ((edge.vertex[1] == i1) && (edge.triangle[0] == edge.triangle[1]))
						{
							edge.triangle[1] = i;
							adjacency[i * 3 + v] = edgeIndex;
							break;
						}
					}

					if (edgeIndex == Unused)
					{
						uint16 edgeIdx = edgeCount++;
						adjacency[i * 3 + v] = edgeIdx;

						Edge& edge = edges[edgeIdx];
						edge.vertex[0] = i1;
						edge.vertex[1] = i2;
						edge.triangle[0] = i;
						edge.triangle[1] = i;
					}
				}
			}
		}
	}
}

TileID CNavMesh::GetNeighbourTileID(size_t x, size_t y, size_t z, size_t side) const
{
	size_t nx = x + NavMesh::GetNeighbourTileOffset(side)[0];
	size_t ny = y + NavMesh::GetNeighbourTileOffset(side)[1];
	size_t nz = z + NavMesh::GetNeighbourTileOffset(side)[2];

	return GetTileID(nx, ny, nz);
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
		return Constants::InvalidTileID;
	}

	return GetTileID(nx, ny, nz);
}

size_t CNavMesh::QueryTriangles(const aabb_t& queryAabbWorld, INavMeshQueryFilter* pOptionalFilter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const
{
	CRY_ASSERT(pOutTriangles);
	CRY_ASSERT(maxTrianglesCount > 0);

	if (!(pOutTriangles && maxTrianglesCount > 0))
	{
		return 0;
	}

	if (pOptionalFilter)
	{
		return QueryTrianglesWithFilterInternal(queryAabbWorld, *pOptionalFilter, maxTrianglesCount, pOutTriangles);
	}
	else
	{
		SAcceptAllQueryTrianglesFilter filter;
		return QueryTrianglesWithFilterInternal(queryAabbWorld, filter, maxTrianglesCount, pOutTriangles);
	}
}

TriangleID CNavMesh::FindClosestTriangle(const vector3_t& queryPosWorld, const TriangleID* pCandidateTriangles, const size_t candidateTrianglesCount, vector3_t* pOutClosestPosWorld, float* pOutClosestDistanceSq) const
{
	CRY_ASSERT(pCandidateTriangles);
	CRY_ASSERT(candidateTrianglesCount > 0);

	if (!(pCandidateTriangles && candidateTrianglesCount > 0))
	{
		return Constants::InvalidTriangleID;
	}

	real_t::unsigned_overflow_type closestDistanceSq;
	const TriangleID resultTriangleId = FindClosestTriangleInternal(queryPosWorld, pCandidateTriangles, candidateTrianglesCount, pOutClosestPosWorld, &closestDistanceSq);
	if (resultTriangleId != Constants::InvalidTriangleID)
	{
		if (pOutClosestDistanceSq)
		{
			// Can't assign overflow_type directly to real_t, but it's still at the same scale, so we can calculate float from it.
			// See fixed_t::as_float().
			*pOutClosestDistanceSq = (closestDistanceSq / (float)real_t::integer_scale);
		}
	}
	return resultTriangleId;
}

bool CNavMesh::GetTileData(const TileID tileId, Tile::STileData& outTileData) const
{
	if (tileId)
	{
		const TileContainer& container = m_tiles[tileId - 1];

		const vector3_t origin(
		  real_t(container.x * m_params.tileSize.x),
		  real_t(container.y * m_params.tileSize.y),
		  real_t(container.z * m_params.tileSize.z));

		container.tile.GetTileData(outTileData);
		outTileData.tileGridCoord = vector3_t(
		  real_t(container.x),
		  real_t(container.y),
		  real_t(container.z));
		outTileData.tileOriginWorld = origin;

		return true;
	}
	return false;
}

static const size_t MaxTriangleCount = 1024;

struct SideTileInfo
{
	SideTileInfo()
		: tile(0)
	{
	}

	STile*    tile;
	vector3_t offset;
};

#pragma warning (push)
#pragma warning (disable: 6262)
void CNavMesh::ComputeAdjacency(size_t x, size_t y, size_t z, const real_t& toleranceSq, STile& tile)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const size_t vertexCount = tile.vertexCount;
	const Tile::Vertex* vertices = tile.GetVertices();

	const size_t triCount = tile.triangleCount;
	if (!triCount)
		return;

	m_profiler.StartTimer(NetworkConstruction);

	assert(triCount <= MaxTriangleCount);

	Edge edges[MaxTriangleCount * 3];
	uint16 adjacency[MaxTriangleCount * 3];
	ComputeTileTriangleAdjacency(tile.GetTriangles(), triCount, vertexCount, edges, adjacency);

	const size_t MaxLinkCount = MaxTriangleCount * 6;
	Tile::SLink links[MaxLinkCount];
	size_t linkCount = 0;

	SideTileInfo sides[SideCount];

	for (size_t s = 0; s < SideCount; ++s)
	{
		SideTileInfo& side = sides[s];

		if (TileID id = GetTileID(x + NavMesh::GetNeighbourTileOffset(s)[0], y + NavMesh::GetNeighbourTileOffset(s)[1], z + NavMesh::GetNeighbourTileOffset(s)[2]))
		{
			side.tile = &GetTile(id);
			side.offset = vector3_t(
			  NavMesh::GetNeighbourTileOffset(s)[0] * m_params.tileSize.x,
			  NavMesh::GetNeighbourTileOffset(s)[1] * m_params.tileSize.y,
			  NavMesh::GetNeighbourTileOffset(s)[2] * m_params.tileSize.z);
		}
	}

	for (size_t i = 0; i < triCount; ++i)
	{
		size_t triLinkCount = 0;

		for (size_t e = 0; e < 3; ++e)
		{
			const size_t edgeIndex = adjacency[i * 3 + e];
			const Edge& edge = edges[edgeIndex];
			if ((edge.triangle[0] != i) && (edge.triangle[1] != i))
				continue;

			if (edge.triangle[0] != edge.triangle[1])
			{
				Tile::SLink& link = links[linkCount++];
				link.side = Tile::SLink::Internal;
				link.edge = e;
				link.triangle = (edge.triangle[1] == i) ? edge.triangle[0] : edge.triangle[1];

				++triLinkCount;
			}
		}

		if (triLinkCount == 3)
		{
			Tile::STriangle& triangle = tile.triangles[i];
			triangle.linkCount = triLinkCount;
			triangle.firstLink = linkCount - triLinkCount;

			continue;
		}

		for (size_t e = 0; e < 3; ++e)
		{
			const size_t edgeIndex = adjacency[i * 3 + e];
			const Edge& edge = edges[edgeIndex];
			if ((edge.triangle[0] != i) && (edge.triangle[1] != i))
				continue;

			if (edge.triangle[0] != edge.triangle[1])
				continue;

			const vector3_t a0 = vector3_t(vertices[edge.vertex[0]]);
			const vector3_t a1 = vector3_t(vertices[edge.vertex[1]]);

			for (size_t s = 0; s < SideCount; ++s)
			{
				if (const STile* stile = sides[s].tile)
				{
					const vector3_t& offset = sides[s].offset;

					const size_t striangleCount = stile->triangleCount;
					const Tile::STriangle* striangles = stile->GetTriangles();
					const Tile::Vertex* svertices = stile->GetVertices();

					for (size_t k = 0; k < striangleCount; ++k)
					{
						const Tile::STriangle& striangle = striangles[k];

						for (size_t ne = 0; ne < 3; ++ne)
						{
							const vector3_t b0 = offset + vector3_t(svertices[striangle.vertex[ne]]);
							const vector3_t b1 = offset + vector3_t(svertices[striangle.vertex[next_mod3(ne)]]);

							if (TestEdgeOverlap(s, toleranceSq, a0, a1, b0, b1))
							{
								Tile::SLink& link = links[linkCount++];
								link.side = s;
								link.edge = e;
								link.triangle = k;

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
								const TileID checkId = GetTileID(x + NavMesh::GetNeighbourTileOffset(s)[0], y + NavMesh::GetNeighbourTileOffset(s)[1], z + NavMesh::GetNeighbourTileOffset(s)[2]);
								m_tiles.BreakOnInvalidTriangle(ComputeTriangleID(checkId, k));
#endif

								++triLinkCount;
								break;
							}
						}
					}
				}
			}
		}

		Tile::STriangle& triangle = tile.triangles[i];
		triangle.linkCount = triLinkCount;
		triangle.firstLink = linkCount - triLinkCount;
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
		ComputeAdjacency(x, y, z, toleranceSq, tile);
	else
	{
		const vector3_t noffset = vector3_t(
		  NavMesh::GetNeighbourTileOffset(side)[0] * m_params.tileSize.x,
		  NavMesh::GetNeighbourTileOffset(side)[1] * m_params.tileSize.y,
		  NavMesh::GetNeighbourTileOffset(side)[2] * m_params.tileSize.z);

		assert(originTriangleCount <= MaxTriangleCount);

		const size_t MaxLinkCount = MaxTriangleCount * 6;
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
				const vector3_t a1 = vector3_t(vertices[originTriangle.vertex[next_mod3(e)]]);

				for (size_t k = 0; k < targetTriangleCount; ++k)
				{
					const Tile::STriangle& ttriangle = targetTriangles[k];

					for (size_t ne = 0; ne < 3; ++ne)
					{
						const vector3_t b0 = noffset + vector3_t(targetVertices[ttriangle.vertex[ne]]);
						const vector3_t b1 = noffset + vector3_t(targetVertices[ttriangle.vertex[next_mod3(ne)]]);

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
	           Re                 When none of the values of 'leaving' intersection parameters is bigger than 1.0, it means, that the ray is ending inside the triangle.
			                      Note: Relation between 'det' and intersection parameters is described in DetailedIntersectSegmentSegment function.
	*/
	
	rayIntersectionParam = 1.0f;
	intersectingEdgeIndex = uint16(MNM::Constants::InvalidEdgeIndex);

	const vector2_t rayStartPos = vector2_t(rayStartPos3D);
	const vector2_t rayEndPos = vector2_t(rayEndPos3D);
	const vector2_t rayDir = rayEndPos - rayStartPos;
	const real_t tolerance = real_t::epsilon();
	const real_t minAllowedValue = real_t(0.0f) - tolerance;

	bool bEndingInside = true;

	for (uint16 edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
	{
		const vector2_t& edgeStartPos = pTriangleVertices[edgeIndex];
		const vector2_t& edgeEndPos = pTriangleVertices[inc_mod3[edgeIndex]];

		const vector2_t edgeDir = edgeEndPos - edgeStartPos;
		const vector2_t diff = edgeStartPos - rayStartPos;

		const real_t det = rayDir.x * edgeDir.y - rayDir.y * edgeDir.x;
		const real_t n = (diff.x * edgeDir.y - diff.y * edgeDir.x);
		
		if (det > 0)
		{
			// Ray is possibly leaving the triangle through this edge line

			// Modified version of n/det < rayIntersectionParam condition because signed division in fixed point aritmetics is not reliable for small values of divisor
			// (dividing positive number with negative can lead to positive result)
			if (n < rayIntersectionParam * det)
			{
				bEndingInside = false;

				// Make sure that the intersections really lies on the edge segment (only checking for the nearest intersection isn't enough because of precision and rounding errors)
				const real_t m = (diff.x * rayDir.y - diff.y * rayDir.x);
				const real_t maxAllowedValue = (real_t(1.0f) * det) + tolerance;

				if (m >= minAllowedValue && m <= maxAllowedValue)
				{
					rayIntersectionParam = n / det;
					intersectingEdgeIndex = edgeIndex;
				}
			}
		}
		else if (det == 0 && n < minAllowedValue)
		{
			// Ray segment is parallel to this edge and is completely outside of the triangle
			bEndingInside = false;
		}
	}
	return bEndingInside;
}

//////////////////////////////////////////////////////////////////////////

#if MNM_USE_EXPORT_INFORMATION

void CNavMesh::ResetAccessibility(uint8 accessible)
{
	for (TileMap::iterator tileIt = m_tileMap.begin(); tileIt != m_tileMap.end(); ++tileIt)
	{
		STile& tile = m_tiles[tileIt->second - 1].tile;

		tile.ResetConnectivity(accessible);
	}
}

void CNavMesh::ComputeAccessibility(const AccessibilityRequest& inputRequest)
{
	struct Node
	{
		Node(TriangleID _id, TriangleID _previousID)
			: id(_id)
			, previousId(_previousID)
		{

		}

		TriangleID id;
		TriangleID previousId;
	};

	std::vector<TriangleID> nextTriangles;
	nextTriangles.reserve(16);

	std::vector<Node> openNodes;
	openNodes.reserve(m_triangleCount);

	if (const TileID startTileID = ComputeTileID(inputRequest.fromTriangle))
	{
		const uint16 startTriangleIdx = ComputeTriangleIndex(inputRequest.fromTriangle);
		STile& startTile = m_tiles[startTileID - 1].tile;
		startTile.SetTriangleAccessible(startTriangleIdx);

		openNodes.push_back(Node(inputRequest.fromTriangle, 0));

		while (!openNodes.empty())
		{
			const Node currentNode = openNodes.back();
			openNodes.pop_back();

			if (const TileID tileID = ComputeTileID(currentNode.id))
			{
				const uint16 triangleIdx = ComputeTriangleIndex(currentNode.id);

				TileContainer& container = m_tiles[tileID - 1];
				STile& tile = container.tile;

				const Tile::STriangle& triangle = tile.triangles[triangleIdx];

				nextTriangles.clear();

				// Collect all accessible nodes from the current one
				for (size_t l = 0; l < triangle.linkCount; ++l)
				{
					const Tile::SLink& link = tile.links[triangle.firstLink + l];

					TriangleID nextTriangleID;

					if (link.side == Tile::SLink::Internal)
					{
						nextTriangleID = ComputeTriangleID(tileID, link.triangle);
					}
					else if (link.side != Tile::SLink::OffMesh)
					{
						TileID neighbourTileID = GetNeighbourTileID(container.x, container.y, container.z, link.side);
						nextTriangleID = ComputeTriangleID(neighbourTileID, link.triangle);
					}
					else
					{
						OffMeshNavigation::QueryLinksResult links = inputRequest.offMeshNavigation.GetLinksForTriangle(currentNode.id, link.triangle);
						while (WayTriangleData nextLink = links.GetNextTriangle())
						{
							nextTriangles.push_back(nextLink.triangleID);
						}
						continue;
					}

					nextTriangles.push_back(nextTriangleID);
				}

				// Add them to the open list if not visited already
				for (size_t t = 0; t < nextTriangles.size(); ++t)
				{
					const TriangleID nextTriangleID = nextTriangles[t];

					// Skip if previous triangle
					if (nextTriangleID == currentNode.previousId)
						continue;

					// Skip if already visited
					if (const TileID nextTileID = ComputeTileID(nextTriangleID))
					{
						STile& nextTile = m_tiles[nextTileID - 1].tile;
						const uint16 nextTriangleIndex = ComputeTriangleIndex(nextTriangleID);
						if (nextTile.IsTriangleAccessible(nextTriangleIndex))
							continue;

						nextTile.SetTriangleAccessible(nextTriangleIndex);

						openNodes.push_back(Node(nextTriangleID, currentNode.id));
					}
				}
			}
		}
	}
}

#endif
}
