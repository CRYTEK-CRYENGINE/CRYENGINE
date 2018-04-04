// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

enum ENavigationIDTag
{
	MeshIDTag = 0,
	AgentTypeIDTag,
	VolumeIDTag,
	TileGeneratorExtensionIDTag,
};

template<ENavigationIDTag T>
struct TNavigationID
{
	explicit TNavigationID(uint32 id = 0) : id(id) {}

	TNavigationID& operator=(const TNavigationID& other)        { id = other.id; return *this; }

	ILINE          operator uint32() const                      { return id; }
	ILINE bool     operator==(const TNavigationID& other) const { return id == other.id; }
	ILINE bool     operator!=(const TNavigationID& other) const { return id != other.id; }
	ILINE bool     operator<(const TNavigationID& other) const  { return id < other.id; }

private:
	uint32 id;
};

typedef TNavigationID<MeshIDTag>                   NavigationMeshID;
typedef TNavigationID<AgentTypeIDTag>              NavigationAgentTypeID;
typedef TNavigationID<VolumeIDTag>                 NavigationVolumeID;
typedef TNavigationID<TileGeneratorExtensionIDTag> TileGeneratorExtensionID;
