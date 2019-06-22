// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "NavigationIdTypes.h"

namespace MNM
{

//! Template structure that is combining values of area 'type' (index) and 'flags' in one variable.
//! areaTypeBitCount template parameter specifies how many bits of the BaseType should be used for storing 'type',
//! remaining bits are then used for 'flags'. 
//! Example: 
//! SAreaTypeAndFlags<uint32, 6> uses uint32 as base type. From that area type uses 6 bits (= 64 values from 0 to 63) and there can be 26 different flags
template<typename BaseType, size_t areaTypeBitCount>
struct SAreaTypeAndFlags
{
	typedef BaseType value_type;

	enum { bit_size = sizeof(value_type) * 8, };
	enum { area_bitcount = areaTypeBitCount, };
	enum { flags_bitcount = bit_size - area_bitcount, };
	enum : value_type { flags_bitmask = (value_type(1) << flags_bitcount) - 1, };
	enum : value_type { area_bitmask = ~flags_bitmask, };
	
	SAreaTypeAndFlags() : value(0) {}
	SAreaTypeAndFlags(const value_type& value) : value(value) {}
	SAreaTypeAndFlags(const SAreaTypeAndFlags& other)
		: value(other.value)
	{}

	bool operator==(const SAreaTypeAndFlags& other) const
	{
		return other.value == value;
	}

	value_type GetFlags() const { return value & flags_bitmask; }
	void SetFlags(value_type flags) { value = (value & area_bitmask) | (flags & flags_bitmask); }

	value_type GetType() const { return value >> flags_bitcount; }
	void SetType(value_type type) { value = (value & flags_bitmask) | (type << flags_bitcount); }

	value_type GetRawValue() const { return value; }

	static constexpr size_t MaxAreasCount() { return value_type(1) << area_bitcount; }
	static constexpr size_t MaxFlagsCount() { return flags_bitcount; }

private:
	value_type value;
};
typedef SAreaTypeAndFlags<uint32, 6> AreaAnnotation;

namespace Constants
{
enum Edges { InvalidEdgeIndex = ~0u };

enum EStaticIsland : uint32
{
	eStaticIsland_InvalidIslandID    = 0,
	eStaticIsland_FirstValidIslandID = 1,
	eStaticIsland_VisitedFlag        = 0x40000000,
	eStaticIsland_UpdatingFlag       = 0x80000000,
};
enum EGlobalIsland
{
	eGlobalIsland_InvalidIslandID = 0,
};

}   // namespace Constants


typedef TNavigationID<TileIDTag>         TileID;
typedef TNavigationID<TileTriangleIDTag> TriangleID;
typedef TNavigationID<OffMeshLinkIDTag>  OffMeshLinkID;

struct SPointOnNavMesh
{
	SPointOnNavMesh() {}
	
	SPointOnNavMesh(const TriangleID triangleId, const Vec3& worldPos)
		: triangleId(triangleId)
		, worldPos(worldPos)
	{}

	bool IsValid() const 
	{ 
		return triangleId.IsValid();
	}

	TriangleID GetTriangleID() const { return triangleId; }
	const Vec3& GetWorldPosition() const { return worldPos; }

private:
	TriangleID triangleId;
	Vec3 worldPos;
};

//! TileTriangleIndex identifies the index of each triangle within a tile
typedef uint16 TileTriangleIndex;

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

// #MNM_TODO pavloi 2016.07.26: replace MNM::TriangleID with CTileTriangleID and provide these functions as member getters (and consider using bit field)
inline MNM::TriangleID ComputeTriangleID(const MNM::TileID tileID, const uint16 triangleIdx)
{
	return MNM::TriangleID((tileID.GetValue() << 10) | (triangleIdx & ((1 << 10) - 1)));
}

inline MNM::TileID ComputeTileID(const MNM::TriangleID triangleID)
{
	return MNM::TileID(triangleID.GetValue() >> 10);
}

inline TileTriangleIndex ComputeTriangleIndex(const MNM::TriangleID triangleID)
{
	return triangleID.GetValue() & ((1 << 10) - 1);
}

} // namespace MNM
