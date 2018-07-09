// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

enum ENavigationIDTag
{
	MeshIDTag = 0,
	AgentTypeIDTag,
	VolumeIDTag,
	TileGeneratorExtensionIDTag,
	AreaTypeIDTag,
	AreaFlagIDTag,
};

template<ENavigationIDTag T, uint32 valueInvalid = 0>
struct TNavigationID
{
	explicit TNavigationID(uint32 id = valueInvalid) : id(id) {}

	TNavigationID& operator=(const TNavigationID& other)        { id = other.id; return *this; }
	ILINE          operator uint32() const                      { return id; } //TODO: remove this operator and add function for returning uint32 value
	ILINE bool     operator==(const TNavigationID& other) const { return id == other.id; }
	ILINE bool     operator!=(const TNavigationID& other) const { return id != other.id; }
	ILINE bool     operator<(const TNavigationID& other) const  { return id < other.id; }
	ILINE bool     IsValid() const { return id != valueInvalid; }
private:
	uint32 id;
};

//TODO: Don't change 'valueInvalid' for other types than NavigationAreaTypeID and NavigationAreaFlagID,
// before all locations are checked in the code where the value is validated using operator uint32()
typedef TNavigationID<MeshIDTag>                   NavigationMeshID;
typedef TNavigationID<AgentTypeIDTag>              NavigationAgentTypeID;
typedef TNavigationID<VolumeIDTag>                 NavigationVolumeID;
typedef TNavigationID<TileGeneratorExtensionIDTag> TileGeneratorExtensionID;
typedef TNavigationID<AreaTypeIDTag, -1>           NavigationAreaTypeID;
typedef TNavigationID<AreaFlagIDTag, -1>           NavigationAreaFlagID;

namespace std
{
template <ENavigationIDTag T, uint32 valueInvalid>
struct hash<TNavigationID<T, valueInvalid>>
{
	size_t operator () (const TNavigationID<T, valueInvalid>& value) const
	{
		std::hash<uint32> hasher;
		return hasher(value);
	}
};
}
