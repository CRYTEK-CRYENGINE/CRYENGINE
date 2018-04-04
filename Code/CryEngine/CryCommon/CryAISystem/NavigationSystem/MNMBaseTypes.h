// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

namespace MNM
{
namespace Constants
{
enum Edges { InvalidEdgeIndex = ~0u };

enum TileIdConstants { InvalidTileID = 0, };
enum TriangleIDConstants { InvalidTriangleID = 0, };

enum EStaticIsland
{
	eStaticIsland_InvalidIslandID    = 0,
	eStaticIsland_FirstValidIslandID = 1,
};
enum EGlobalIsland
{
	eGlobalIsland_InvalidIslandID = 0,
};

enum EOffMeshLink
{
	eOffMeshLinks_InvalidOffMeshLinkID = 0,
};

}   // namespace Constants

// Basic types used in the MNM namespace.
// #MNM_TODO pavloi 2016.07.19: convert to use strong typed wrappers instead of typedefs (like TNavigationID)
typedef uint32 TileID;
typedef uint32 TriangleID; // #MNM_TODO pavloi 2016.07.19: actually, it means TileTriangleID
typedef uint32 OffMeshLinkID;

//! StaticIslandIDs identify triangles that are statically connected inside a mesh and that are reachable without the use of any off mesh links.
typedef uint32 StaticIslandID;

//! GlobalIslandIDs define IDs able to code and connect islands between meshes.
struct GlobalIslandID
{
	GlobalIslandID(const uint64 defaultValue = MNM::Constants::eGlobalIsland_InvalidIslandID)
		: id(defaultValue)
	{
		static_assert(sizeof(StaticIslandID) <= 4, "The maximum supported size for StaticIslandIDs is 4 bytes.");
	}

	GlobalIslandID(const uint32 navigationMeshID, const MNM::StaticIslandID islandID)
	{
		id = ((uint64)navigationMeshID << 32) | islandID;
	}

	GlobalIslandID operator=(const GlobalIslandID other)
	{
		id = other.id;
		return *this;
	}

	inline bool operator==(const GlobalIslandID& rhs) const
	{
		return id == rhs.id;
	}

	inline bool operator<(const GlobalIslandID& rhs) const
	{
		return id < rhs.id;
	}

	MNM::StaticIslandID GetStaticIslandID() const
	{
		return (MNM::StaticIslandID) (id & ((uint64)1 << 32) - 1);
	}

	uint32 GetNavigationMeshIDAsUint32() const
	{
		return static_cast<uint32>(id >> 32);
	}

	uint64 id;
};

// #MNM_TODO pavloi 2016.07.26: replace TriangleID with CTileTriangleID and provide these functions as member getters (and consider using bit field)
inline TriangleID ComputeTriangleID(TileID tileID, uint16 triangleIdx)
{
	return (tileID << 10) | (triangleIdx & ((1 << 10) - 1));
}

inline TileID ComputeTileID(TriangleID triangleID)
{
	return triangleID >> 10;
}

inline uint16 ComputeTriangleIndex(TriangleID triangleID)
{
	return triangleID & ((1 << 10) - 1);
}

} // namespace MNM
