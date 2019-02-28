// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Islands.h"

#include "NavMesh.h"
#include "IslandConnections.h"
#include "../NavigationSystem/OffMeshNavigationManager.h"

namespace MNM
{

inline CIslands::TwoStaticIslandsIDs CombineIslandIds(const StaticIslandID id1, const StaticIslandID id2)
{
	CRY_ASSERT(id1 >= MNM::Constants::eStaticIsland_FirstValidIslandID);
	CRY_ASSERT(id2 >= MNM::Constants::eStaticIsland_FirstValidIslandID);

	static_assert(sizeof(id1) + sizeof(id2) <= sizeof(CIslands::TwoStaticIslandsIDs), "Unsupported StaticIslandID size!");
	
	return (CIslands::TwoStaticIslandsIDs(id1) << 32) | id2;
}

inline void RestoreIslandIds(const uint64 combinedValue, StaticIslandID& id1, StaticIslandID& id2)
{
	static_assert(sizeof(id1) == 4, "Unsupported StaticIslandID size!");
	
	id1 = StaticIslandID(combinedValue >> 32);
	id2 = StaticIslandID(combinedValue);
}

inline StaticIslandID GetIslandID(StaticIslandID id)
{
	return id & ~(MNM::Constants::eStaticIsland_VisitedFlag | MNM::Constants::eStaticIsland_UpdatingFlag);
}

inline bool IsIslandVisited(StaticIslandID id)
{
	return (id & MNM::Constants::eStaticIsland_VisitedFlag) != 0;
}

//////////////////////////////////////////////////////////////////////////

CIslands::CIslands()
{
	m_islands.reserve(32);
	m_islandsFreeIndices.reserve(4);
}

void CIslands::ComputeStaticIslandsAndConnections(CNavMesh& navMesh, const NavigationMeshID meshID, const OffMeshNavigationManager& offMeshNavigationManager, MNM::IslandConnections& islandConnections)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	ResetConnectedIslandsIDs(navMesh);

	SConnectionRequests connectionRequests;
	ComputeStaticIslands(navMesh, connectionRequests);
	ResolvePendingConnectionRequests(navMesh, connectionRequests, meshID, &offMeshNavigationManager, islandConnections);
}

float CIslands::GetIslandArea(StaticIslandID islandID) const
{
	bool isValid = (islandID >= MNM::Constants::eStaticIsland_FirstValidIslandID && (GetIslandIndex(islandID)) < m_islands.size());
	return isValid ? m_islands[GetIslandIndex(islandID)].area : -1.f;
}

void CIslands::ResetConnectedIslandsIDs(CNavMesh& navMesh)
{
	for (CNavMesh::TileMap::iterator tileIt = navMesh.m_tileMap.begin(); tileIt != navMesh.m_tileMap.end(); ++tileIt)
	{
		STile& tile = navMesh.m_tiles[tileIt->second - 1].tile;

		for (uint16 i = 0; i < tile.triangleCount; ++i)
		{
			tile.triangles[i].islandID = MNM::Constants::eStaticIsland_InvalidIslandID;
		}
	}

	m_islands.clear();
	m_islandsFreeIndices.clear();
}

void CIslands::FloodFillOnTriangles(CNavMesh& navMesh, const MNM::TriangleID sourceTriangleId, const size_t reserveCount, 
	std::function<bool(const STile& prevTile, const Tile::STriangle& prevTriangle, const STile& nextTile, Tile::STriangle& nextTriangle)> executeFunc)
{
	std::vector<TriangleID> trianglesToVisit;
	trianglesToVisit.reserve(reserveCount);

	trianglesToVisit.push_back(sourceTriangleId);

	while (!trianglesToVisit.empty())
	{
		const MNM::TriangleID currentTriangleID = trianglesToVisit.back();
		trianglesToVisit.pop_back();

		const TileID currentTileId = ComputeTileID(currentTriangleID);
		const CNavMesh::TileContainer& currentContainer = navMesh.m_tiles[currentTileId - 1];
		const STile& currentTile = currentContainer.tile;
		Tile::STriangle& currentTriangle = currentTile.triangles[ComputeTriangleIndex(currentTriangleID)];

		for (uint16 l = 0; l < currentTriangle.linkCount; ++l)
		{
			const Tile::SLink& link = currentTile.links[currentTriangle.firstLink + l];
			if (link.side == Tile::SLink::OffMesh)
				continue;

			const TileID nextTileId = link.side == Tile::SLink::Internal ? currentTileId : navMesh.GetNeighbourTileID(currentContainer.x, currentContainer.y, currentContainer.z, link.side);
			const STile& nextTile = navMesh.m_tiles[nextTileId - 1].tile;
			Tile::STriangle& nextTriangle = nextTile.triangles[link.triangle];

			if (executeFunc(currentTile, currentTriangle, nextTile, nextTriangle))
			{
				trianglesToVisit.push_back(ComputeTriangleID(nextTileId, link.triangle));
			}
		}
	}
}

template<typename T>
void CIslands::FloodFillOnTrianglesWithBackupValue(CNavMesh& navMesh, const MNM::TriangleID sourceTriangleId, const T sourceValue, const size_t reserveCount,
	std::function<bool(const STile& prevTile, const Tile::STriangle& prevTriangle, const T prevValue, const STile& nextTile, Tile::STriangle& nextTriangle, T& nextValue)> executeFunc)
{
	struct STriangleWithValue
	{
		STriangleWithValue(const TriangleID triangleId, const T value) : triangleId(triangleId), value(value) {}
		
		TriangleID triangleId;
		T value;
	};
	
	std::vector<STriangleWithValue> trianglesToVisit;
	trianglesToVisit.reserve(reserveCount);

	trianglesToVisit.emplace_back(sourceTriangleId, sourceValue );

	while (!trianglesToVisit.empty())
	{
		const STriangleWithValue currentTriangleIDwithValue = trianglesToVisit.back();
		trianglesToVisit.pop_back();

		const TileID currentTileId = ComputeTileID(currentTriangleIDwithValue.triangleId);
		const CNavMesh::TileContainer& currentContainer = navMesh.m_tiles[currentTileId - 1];
		const STile& currentTile = currentContainer.tile;
		Tile::STriangle& currentTriangle = currentTile.triangles[ComputeTriangleIndex(currentTriangleIDwithValue.triangleId)];

		for (uint16 l = 0; l < currentTriangle.linkCount; ++l)
		{
			const Tile::SLink& link = currentTile.links[currentTriangle.firstLink + l];
			if (link.side == Tile::SLink::OffMesh)
				continue;

			const TileID nextTileId = link.side == Tile::SLink::Internal ? currentTileId : navMesh.GetNeighbourTileID(currentContainer.x, currentContainer.y, currentContainer.z, link.side);
			const STile& nextTile = navMesh.m_tiles[nextTileId - 1].tile;
			Tile::STriangle& nextTriangle = nextTile.triangles[link.triangle];

			T nextPrevValue;
			if (executeFunc(currentTile, currentTriangle, currentTriangleIDwithValue.value, nextTile, nextTriangle, nextPrevValue))
			{
				trianglesToVisit.emplace_back( ComputeTriangleID(nextTileId, link.triangle), nextPrevValue );
			}
		}
	}
}

void CIslands::ComputeStaticIslands(CNavMesh& navMesh, SConnectionRequests& connectionRequests)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	std::vector<TriangleID> trianglesToVisit;
	trianglesToVisit.reserve(4096);

	// The idea is to iterate through the existing tiles and then finding the triangles from which
	// start the assigning of the different island ids.
	for (CNavMesh::TileMap::iterator tileIt = navMesh.m_tileMap.begin(); tileIt != navMesh.m_tileMap.end(); ++tileIt)
	{
		const TileID tileID = tileIt->second;
		const STile& tile = navMesh.m_tiles[tileID - 1].tile;
		for (uint16 triangleIndex = 0; triangleIndex < tile.triangleCount; ++triangleIndex)
		{
			Tile::STriangle& sourceTriangle = tile.triangles[triangleIndex];
			if (sourceTriangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID)
			{
				continue;
			}

			SIsland& newIsland = GetNewIsland(sourceTriangle.areaAnnotation);
			++newIsland.trianglesCount;
			newIsland.area += tile.GetTriangleArea(sourceTriangle).as_float();
			sourceTriangle.islandID = newIsland.id;
			const TriangleID triangleID = ComputeTriangleID(tileID, triangleIndex);

			trianglesToVisit.push_back(triangleID);

			// We now have another triangle to start from to assign the new island ids to all the connected
			// triangles
			size_t totalTrianglesToVisit = 1;
			for (size_t index = 0; index < totalTrianglesToVisit; ++index)
			{
				// Get next triangle to start the evaluation from
				const MNM::TriangleID currentTriangleID = trianglesToVisit[index];

				const TileID currentTileId = ComputeTileID(currentTriangleID);
				CRY_ASSERT_MESSAGE(currentTileId > 0, "ComputeStaticIslands is trying to access a triangle ID associated with an invalid tile id.");

				const CNavMesh::TileContainer& container = navMesh.m_tiles[currentTileId - 1];
				const STile& currentTile = container.tile;
				const Tile::STriangle& currentTriangle = currentTile.GetTriangles()[ComputeTriangleIndex(currentTriangleID)];

				for (uint16 l = 0; l < currentTriangle.linkCount; ++l)
				{
					const Tile::SLink& link = currentTile.links[currentTriangle.firstLink + l];
					if (link.side != Tile::SLink::OffMesh)
					{
						const TileID nextTileId = link.side == Tile::SLink::Internal ? currentTileId : navMesh.GetNeighbourTileID(container.x, container.y, container.z, link.side);
						const STile& nextTile = navMesh.m_tiles[nextTileId - 1].tile;
						Tile::STriangle& nextTriangle = nextTile.triangles[link.triangle];

						if (nextTriangle.islandID == MNM::Constants::eStaticIsland_InvalidIslandID)
						{
							if (nextTriangle.areaAnnotation == currentTriangle.areaAnnotation)
							{
								++totalTrianglesToVisit;
								++newIsland.trianglesCount;
								newIsland.area += nextTile.GetTriangleArea(nextTriangle).as_float();
								nextTriangle.islandID = newIsland.id;
								trianglesToVisit.push_back(ComputeTriangleID(nextTileId, link.triangle));
							}
						}
						else if (nextTriangle.islandID != currentTriangle.islandID)
						{
							connectionRequests.AddAreaConnection(newIsland.id, nextTriangle.islandID);
						}
					}
					else
					{
						connectionRequests.AddOffmeshConnection(currentTriangle.islandID, currentTriangleID, link.triangle);
					}
				}
			}
			trianglesToVisit.clear();
		}
	}
}

void CIslands::UpdateIslandsForTriangles(CNavMesh& navMesh, const NavigationMeshID meshID, const MNM::TriangleID* pTrianglesArray, const size_t trianglesCount, MNM::IslandConnections& islandConnections)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	
	SConnectionRequests connectionRequests;

	std::unordered_map<StaticIslandID, std::vector<MNM::TriangleID>> splitUpdateSources;

	//////////////////////////////////////////////////////////////////////////
	// Get source triangles for updating
	//////////////////////////////////////////////////////////////////////////
	for (size_t i = 0; i < trianglesCount; ++i)
	{
		const MNM::TriangleID triangleID = pTrianglesArray[i];

		const TileID currentTileId = ComputeTileID(triangleID);
		const CNavMesh::TileContainer& currentContainer = navMesh.m_tiles[currentTileId - 1];
		const STile& currentTile = currentContainer.tile;
		Tile::STriangle& currentTriangle = currentTile.triangles[ComputeTriangleIndex(triangleID)];

		if (currentTriangle.islandID == MNM::Constants::eStaticIsland_InvalidIslandID)
			continue; // This may currently happen when the tile is regenerated and islands aren't recomputed from scratch yet

		const StaticIslandID currentIslandId = GetIslandID(currentTriangle.islandID);
		currentTriangle.islandID = MNM::Constants::eStaticIsland_InvalidIslandID;

		SIsland& island = GetIsland(currentIslandId);
		island.area -= currentTile.GetTriangleArea(currentTriangle).as_float();
		if (--island.trianglesCount == 0)
		{
			ReleaseIsland(island);
		}
		CRY_ASSERT(island.trianglesCount >= 0);

		for (uint16 l = 0; l < currentTriangle.linkCount; ++l)
		{
			const Tile::SLink& link = currentTile.links[currentTriangle.firstLink + l];
			if (link.side == Tile::SLink::OffMesh)
				continue;

			const TileID nextTileId = link.side == Tile::SLink::Internal ? currentTileId : navMesh.GetNeighbourTileID(currentContainer.x, currentContainer.y, currentContainer.z, link.side);
			const STile& nextTile = navMesh.m_tiles[nextTileId - 1].tile;
			Tile::STriangle& nextTriangle = nextTile.triangles[link.triangle];
			const StaticIslandID nextIslandId = GetIslandID(nextTriangle.islandID);
			if (nextIslandId != currentIslandId)
			{
				if (nextIslandId != MNM::Constants::eStaticIsland_InvalidIslandID)
				{
					connectionRequests.RemoveAreaConnection(currentIslandId, nextIslandId);
				}
			}
			else if ((nextTriangle.areaAnnotation == currentTriangle.areaAnnotation) == false)
			{
				if ((nextTriangle.islandID & MNM::Constants::eStaticIsland_UpdatingFlag) == 0)
				{
					nextTriangle.islandID |= MNM::Constants::eStaticIsland_UpdatingFlag;
					splitUpdateSources[nextIslandId].emplace_back(ComputeTriangleID(nextTileId, link.triangle));
				}
			}
		}
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Merging islands
	//////////////////////////////////////////////////////////////////////////
	for (size_t i = 0; i < trianglesCount; ++i)
	{
		const MNM::TriangleID sourceTriangleID = pTrianglesArray[i];
		const TileID sourceTileId = ComputeTileID(sourceTriangleID);
		const CNavMesh::TileContainer& sourceContainer = navMesh.m_tiles[sourceTileId - 1];
		const STile& sourceTile = sourceContainer.tile;
		Tile::STriangle& sourceTriangle = sourceTile.triangles[ComputeTriangleIndex(sourceTriangleID)];

		if (sourceTriangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID)
			continue;

		// Try to find right island from the neighboring triangles
		StaticIslandID sourceIslandId = MNM::Constants::eStaticIsland_InvalidIslandID;
		for (uint16 l = 0; l < sourceTriangle.linkCount; ++l)
		{
			const Tile::SLink& link = sourceTile.links[sourceTriangle.firstLink + l];
			if (link.side == Tile::SLink::OffMesh)
				continue;

			const TileID nextTileId = link.side == Tile::SLink::Internal ? sourceTileId : navMesh.GetNeighbourTileID(sourceContainer.x, sourceContainer.y, sourceContainer.z, link.side);
			const STile& nextTile = navMesh.m_tiles[nextTileId - 1].tile;
			Tile::STriangle& nextTriangle = nextTile.triangles[link.triangle];
			if (nextTriangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID && nextTriangle.areaAnnotation == sourceTriangle.areaAnnotation)
			{
				sourceIslandId = nextTriangle.islandID;
				break;
			}
		}

		if (sourceIslandId == MNM::Constants::eStaticIsland_InvalidIslandID)
			continue;

		SIsland& sourceIsland = GetIsland(sourceIslandId);
		++sourceIsland.trianglesCount;
		sourceIsland.area += sourceTile.GetTriangleArea(sourceTriangle).as_float();
		sourceTriangle.islandID = sourceIslandId;

		FloodFillOnTrianglesWithBackupValue<StaticIslandID>(navMesh, sourceTriangleID, MNM::Constants::eStaticIsland_InvalidIslandID, 8,
			[this, &sourceIsland, &connectionRequests](const STile& prevTile, const Tile::STriangle& currentTriangle, const StaticIslandID prevValue, const STile& nextTile, Tile::STriangle& nextTriangle, StaticIslandID& nextPrevValue)
		{
			const StaticIslandID nextIslandId = GetIslandID(nextTriangle.islandID);
			
			if (nextTriangle.areaAnnotation == currentTriangle.areaAnnotation)
			{
				nextPrevValue = nextIslandId;
				if (nextIslandId == MNM::Constants::eStaticIsland_InvalidIslandID)
				{
					nextTriangle.islandID = sourceIsland.id;
					++sourceIsland.trianglesCount;
					sourceIsland.area += nextTile.GetTriangleArea(nextTriangle).as_float();
					return true;
				}
				else if (nextIslandId != currentTriangle.islandID)
				{
					const float triangleArea = nextTile.GetTriangleArea(nextTriangle).as_float();
					
					SIsland& nextIsland = GetIsland(nextIslandId);
					nextTriangle.islandID = sourceIsland.id;
					nextIsland.area -= triangleArea;
					if (--nextIsland.trianglesCount == 0)
					{
						ReleaseIsland(nextIsland);
					}
					++sourceIsland.trianglesCount;
					sourceIsland.area += triangleArea;
					return true;
				}
			}
			else
			{
				CRY_ASSERT(nextIslandId != currentTriangle.islandID);
				if (nextIslandId != MNM::Constants::eStaticIsland_InvalidIslandID)
				{
					if (prevValue != MNM::Constants::eStaticIsland_InvalidIslandID)
					{
						connectionRequests.RemoveAreaConnection(prevValue, nextIslandId);
					}
					connectionRequests.AddAreaConnection(currentTriangle.islandID, nextIslandId);
				}
			}
			return false;
		});
	}

	//////////////////////////////////////////////////////////////////////////
	// Splitting islands
	//////////////////////////////////////////////////////////////////////////
	for (const auto& islandSplitSources : splitUpdateSources)
	{
		const StaticIslandID sourceIslandId = islandSplitSources.first;
		SIsland& sourceIsland = GetIsland(sourceIslandId);

		bool wantCreateNewIsland = false;

		for (const MNM::TriangleID sourceTriangleID : islandSplitSources.second)
		{
			const TileID sourceTileId = ComputeTileID(sourceTriangleID);
			const CNavMesh::TileContainer& sourceContainer = navMesh.m_tiles[sourceTileId - 1];
			const STile& sourceTile = sourceContainer.tile;
			Tile::STriangle& sourceTriangle = sourceTile.triangles[ComputeTriangleIndex(sourceTriangleID)];

			if ((sourceTriangle.islandID & MNM::Constants::eStaticIsland_UpdatingFlag) == 0)
				continue;

			sourceTriangle.islandID = sourceIslandId;
			uint32 visitedCount = 1;

			if (wantCreateNewIsland)
			{
				SIsland& newIsland = GetNewIsland(sourceTriangle.areaAnnotation);

				sourceTriangle.islandID = newIsland.id;
				++newIsland.trianglesCount;
				newIsland.area += sourceTile.GetTriangleArea(sourceTriangle).as_float();

				sourceIsland = GetIsland(sourceIslandId);

				FloodFillOnTriangles(navMesh, sourceTriangleID, 8,
					[sourceIslandId, &newIsland, &connectionRequests](const STile& prevTile, const Tile::STriangle& currentTriangle, const STile& nextTile, Tile::STriangle& nextTriangle)
				{
					const StaticIslandID nextIslandId = GetIslandID(nextTriangle.islandID);
					if (nextIslandId == sourceIslandId)
					{
						++newIsland.trianglesCount;
						newIsland.area += nextTile.GetTriangleArea(nextTriangle).as_float();
						nextTriangle.islandID = newIsland.id;
						return true;
					}
					else if(nextIslandId != newIsland.id && nextIslandId != MNM::Constants::eStaticIsland_InvalidIslandID)
					{
						connectionRequests.RemoveAreaConnection(sourceIslandId, nextIslandId);
						connectionRequests.AddAreaConnection(currentTriangle.islandID, nextIslandId);
					}
					return false;
				});
				sourceIsland.trianglesCount -= newIsland.trianglesCount;
				sourceIsland.area -= newIsland.area;
				CRY_ASSERT(sourceIsland.trianglesCount > 0);
			}
			else
			{
				sourceTriangle.islandID |= MNM::Constants::eStaticIsland_VisitedFlag;
				
				// Count accessible triangles
				FloodFillOnTriangles(navMesh, sourceTriangleID, 8,
					[&sourceIsland, &visitedCount](const STile& prevTile, const Tile::STriangle& currentTriangle, const STile& nextTile, Tile::STriangle& nextTriangle)
				{
					if (GetIslandID(nextTriangle.islandID) == sourceIsland.id && !IsIslandVisited(nextTriangle.islandID))
					{
						nextTriangle.islandID = currentTriangle.islandID;
						++visitedCount;
						return true;
					}
					return false;
				});

				// Visit all triangles again to clear eStaticIsland_VisitedFlag
				sourceTriangle.islandID &= ~MNM::Constants::eStaticIsland_VisitedFlag;
				FloodFillOnTriangles(navMesh, sourceTriangleID, 8,
					[&sourceIsland](const STile& prevTile, const Tile::STriangle& currentTriangle, const STile& nextTile, Tile::STriangle& nextTriangle)
				{
					if (GetIslandID(nextTriangle.islandID) == sourceIsland.id && IsIslandVisited(nextTriangle.islandID))
					{
						nextTriangle.islandID = currentTriangle.islandID;
						return true;
					}
					return false;
				});
				wantCreateNewIsland = sourceIsland.trianglesCount != visitedCount;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Create new islands for unresolved triangles
	//////////////////////////////////////////////////////////////////////////
	for (size_t i = 0; i < trianglesCount; ++i)
	{
		const MNM::TriangleID sourceTriangleID = pTrianglesArray[i];

		const TileID sourceTileId = ComputeTileID(sourceTriangleID);
		const CNavMesh::TileContainer& sourceContainer = navMesh.m_tiles[sourceTileId - 1];
		const STile& sourceTile = sourceContainer.tile;
		Tile::STriangle& sourceTriangle = sourceTile.triangles[ComputeTriangleIndex(sourceTriangleID)];

		if (sourceTriangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID)
			continue;

		SIsland& newIsland = GetNewIsland(sourceTriangle.areaAnnotation);

		sourceTriangle.islandID = newIsland.id;
		++newIsland.trianglesCount;
		newIsland.area = sourceTile.GetTriangleArea(sourceTriangle).as_float();

		FloodFillOnTriangles(navMesh, sourceTriangleID, trianglesCount,
			[&newIsland, &connectionRequests](const STile& prevTile, const Tile::STriangle& currentTriangle, const STile& nextTile, Tile::STriangle& nextTriangle)
		{
			if (nextTriangle.islandID == MNM::Constants::eStaticIsland_InvalidIslandID)
			{
				if (nextTriangle.areaAnnotation == currentTriangle.areaAnnotation)
				{
					nextTriangle.islandID = newIsland.id;
					++newIsland.trianglesCount;
					newIsland.area = nextTile.GetTriangleArea(nextTriangle).as_float();
					return true;
				}
			}
			else if (nextTriangle.islandID != currentTriangle.islandID)
			{
				connectionRequests.AddAreaConnection(currentTriangle.islandID, nextTriangle.islandID);
			}
			return false;
		});
	}

	ResolvePendingConnectionRequests(navMesh, connectionRequests, meshID, nullptr, islandConnections);
}

CIslands::SIsland& CIslands::GetNewIsland(const AreaAnnotation annotation)
{
	CRY_ASSERT(m_islands.size() != std::numeric_limits<StaticIslandID>::max());

	if (m_islandsFreeIndices.size())
	{
		const size_t freeIdx = m_islandsFreeIndices.back();
		m_islandsFreeIndices.pop_back();

		CRY_ASSERT(freeIdx < m_islands.size());
		CRY_ASSERT(m_islands[freeIdx].trianglesCount == 0);

		SIsland& newIsland = m_islands[freeIdx];
		newIsland = SIsland(newIsland.id, annotation);

		return newIsland;
	}

	// Generate new id (NOTE: Invalid is 0)
	const StaticIslandID id = m_islands.size() + 1;

	CRY_ASSERT_MESSAGE(GetIslandID(id) == id, "CIslands::GetNewIsland: Too many islands created!");

	m_islands.emplace_back(id, annotation);
	return m_islands.back();
}

void CIslands::ReleaseIsland(const SIsland& island)
{
	CRY_ASSERT(island.trianglesCount == 0);
	
	m_islandsFreeIndices.push_back(GetIslandIndex(island.id));
}

void CIslands::ResolvePendingConnectionRequests(CNavMesh& navMesh, SConnectionRequests& connectionRequests, const NavigationMeshID meshID, const OffMeshNavigationManager* pOffMeshNavigationManager,
	MNM::IslandConnections& islandConnections)
{
	if (pOffMeshNavigationManager)
	{
		const OffMeshNavigation& offMeshNavigation = pOffMeshNavigationManager->GetOffMeshNavigationForMesh(meshID);

		while (!connectionRequests.offmeshConnections.empty())
		{
			const SConnectionRequests::OffmeshConnectionRequest& request = connectionRequests.offmeshConnections.back();
			const OffMeshNavigation::QueryLinksResult links = offMeshNavigation.GetLinksForTriangle(request.startingTriangleID, request.offMeshLinkIndex);
			while (const WayTriangleData nextTri = links.GetNextTriangle())
			{
				Tile::STriangle endTriangle;
				if (navMesh.GetTriangle(nextTri.triangleID, endTriangle))
				{
					const OffMeshLink* pLink = pOffMeshNavigationManager->GetOffMeshLink(nextTri.offMeshLinkID);
					CRY_ASSERT(pLink);
					const MNM::IslandConnections::Link link(nextTri.triangleID, nextTri.offMeshLinkID, GlobalIslandID(meshID, endTriangle.islandID), endTriangle.areaAnnotation, pLink->GetEntityIdForOffMeshLink(), 1);
					islandConnections.SetOneWayOffmeshConnectionBetweenIslands(GlobalIslandID(meshID, request.startingIslandID), link);
				}
			}
			connectionRequests.offmeshConnections.pop_back();
		}
	}

	for (const auto& keyValue : connectionRequests.areaConnection)
	{
		StaticIslandID islandId1;
		StaticIslandID islandId2;
		RestoreIslandIds(keyValue.first, islandId1, islandId2);
		const int connectionsChange = keyValue.second;
		const SIsland& island1 = GetIsland(islandId1);
		const SIsland& island2 = GetIsland(islandId2);

		islandConnections.SetTwoWayConnectionBetweenIslands(GlobalIslandID(meshID, islandId1), island1.annotation, GlobalIslandID(meshID, islandId2), island2.annotation, connectionsChange);
	}

	connectionRequests.areaConnection.clear();
}

//////////////////////////////////////////////////////////////////////////

void CIslands::SConnectionRequests::AddAreaConnection(const StaticIslandID firstIslandId, const StaticIslandID secondIslandId)
{
	const auto insertIt = areaConnection.insert({ CombineIslandIds(firstIslandId, secondIslandId), 1 });
	if (!insertIt.second)
	{
		insertIt.first->second += 1;
	}
}
void CIslands::SConnectionRequests::RemoveAreaConnection(const StaticIslandID firstIslandId, const StaticIslandID secondIslandId)
{
	const auto insertIt = areaConnection.insert({ CombineIslandIds(firstIslandId, secondIslandId), -1 });
	if (!insertIt.second)
	{
		insertIt.first->second -= 1;
	}
}
void CIslands::SConnectionRequests::AddOffmeshConnection(const StaticIslandID startingIslandId, const TriangleID startingTriangleId, const uint16 offMeshLinkIndex)
{
	offmeshConnections.emplace_back(startingIslandId, startingTriangleId, offMeshLinkIndex);
}

}