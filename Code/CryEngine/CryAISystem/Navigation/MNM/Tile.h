// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_TILE_H
#define __MNM_TILE_H

#pragma once

#include <CryAISystem/NavigationSystem/MNMTile.h>

#include "MNM.h"

namespace MNM
{
struct STile
{
	// #MNM_TODO pavloi 2016.07.22: CNavMesh is friend for time being, so I don't have to fix data access in lot's of places right now.
	friend class CNavMesh;
	friend class CIslands;

	STile();

	const Tile::STriangle* GetTriangles() const      { return triangles; }
	const Tile::Vertex*    GetVertices() const       { return vertices; }
	const Tile::SLink*     GetLinks() const          { return links; }
	const Tile::SBVNode*   GetBVNodes() const        { return nodes; }

	uint16                 GetTrianglesCount() const { return triangleCount; }
	uint16                 GetVerticesCount() const  { return vertexCount; }
	uint16                 GetLinksCount() const     { return linkCount; }
	uint16                 GetBVNodesCount() const   { return nodeCount; }

	uint32                 GetHashValue() const      { return hashValue; }

	void                   GetTileData(Tile::STileData& outTileData) const;

	void                   SetTriangles(std::unique_ptr<Tile::STriangle[]>&& pTriangles, uint16 count);
	void                   SetVertices(std::unique_ptr<Tile::Vertex[]>&& pVertices, uint16 count);
	void                   SetNodes(std::unique_ptr<Tile::SBVNode[]>&& pNodes, uint16 count);
	void                   SetLinks(std::unique_ptr<Tile::SLink[]>&& pLinks, uint16 count);

	void                   SetHashValue(uint32 value);

	void                   CopyTriangles(const Tile::STriangle* triangles, uint16 count);
	void                   CopyVertices(const Tile::Vertex* vertices, uint16 count);
	void                   CopyNodes(const Tile::SBVNode* nodes, uint16 count);
	void                   CopyLinks(const Tile::SLink* links, uint16 count);

	void                   AddOffMeshLink(const TriangleID triangleID, const uint16 offMeshIndex);
	void                   UpdateOffMeshLink(const TriangleID triangleID, const uint16 offMeshIndex);
	void                   RemoveOffMeshLink(const TriangleID triangleID);

	void                   Swap(STile& other);
	void                   Destroy();

	vector3_t::value_type  GetTriangleArea(const TriangleID triangleID) const;
	vector3_t::value_type  GetTriangleArea(const Tile::STriangle& triangle) const;

#if MNM_USE_EXPORT_INFORMATION
	void        ResetConnectivity(uint8 accessible);
	inline bool IsTriangleAccessible(const uint16 triangleIdx) const
	{
		assert((triangleIdx >= 0) && (triangleIdx < connectivity.triangleCount));
		return (connectivity.trianglesAccessible[triangleIdx] != 0);
	}
	inline bool IsTileAccessible() const
	{
		return (connectivity.tileAccessible != 0);
	}

	inline void SetTriangleAccessible(const uint16 triangleIdx)
	{
		assert((triangleIdx >= 0) && (triangleIdx < connectivity.triangleCount));

		connectivity.tileAccessible = 1;
		connectivity.trianglesAccessible[triangleIdx] = 1;
	}
#endif

	enum DrawFlags
	{
		DrawTriangles         = BIT(0),
		DrawInternalLinks     = BIT(1),
		DrawExternalLinks     = BIT(2),
		DrawOffMeshLinks      = BIT(3),
		DrawMeshBoundaries    = BIT(4),
		DrawBVTree            = BIT(5),
		DrawAccessibility     = BIT(6),
		DrawTrianglesId       = BIT(7),
		DrawIslandsId         = BIT(8),
		DrawTriangleBackfaces = BIT(9),
		DrawAll               = ~0ul,
	};

	void Draw(size_t drawFlags, vector3_t origin, TileID tileID, const std::vector<float>& islandAreas, const ITriangleColorSelector& colorSelector) const;

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
	void ValidateTriangles() const;
#else
	void ValidateTriangles() const {}
#endif // DEBUG_MNM_DATA_CONSISTENCY_ENABLED

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
private:
	void ValidateTriangleLinks();
#endif

private:

	// #MNM_TODO pavloi 2016.07.26: consider replacing with unique_ptr.
	// Needs changes in TileContainerArray - it expects STile to be POD without destructor, so it can just memcpy it.
	Tile::STriangle* triangles;
	Tile::Vertex*    vertices;
	Tile::SBVNode*   nodes;
	Tile::SLink*     links;

	uint16           triangleCount;
	uint16           vertexCount;
	uint16           nodeCount;
	uint16           linkCount;

	uint32           hashValue;

#if MNM_USE_EXPORT_INFORMATION

private:
	struct TileConnectivity
	{
		TileConnectivity()
			: tileAccessible(1)
			, trianglesAccessible(NULL)
			, triangleCount(0)
		{
		}

		uint8  tileAccessible;
		uint8* trianglesAccessible;
		uint16 triangleCount;
	};

	bool ConsiderExportInformation() const;
	void InitConnectivity(uint16 oldTriangleCount, uint16 newTriangleCount);

	TileConnectivity connectivity;

#endif
};
}

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
struct CompareLink
{
	CompareLink(const MNM::Tile::SLink& link)
	{
		m_link.edge = link.edge;
		m_link.side = link.side;
		m_link.triangle = link.triangle;
	}

	bool operator()(const MNM::Tile::SLink& other)
	{
		return m_link.edge == other.edge && m_link.side == other.side && m_link.triangle == other.triangle;
	}

	MNM::Tile::SLink m_link;
};

inline void BreakOnMultipleAdjacencyLinkage(const MNM::Tile::SLink* start, const MNM::Tile::SLink* end, const MNM::Tile::SLink& linkToTest)
{
	if (std::count_if(start, end, CompareLink(linkToTest)) > 1)
		__debugbreak();
}
#else
inline void BreakOnMultipleAdjacencyLinkage(const MNM::Tile::SLink* start, const MNM::Tile::SLink* end, const MNM::Tile::SLink& linkToTest)
{

}
#endif

#endif  // #ifndef __MNM_TILE_H
