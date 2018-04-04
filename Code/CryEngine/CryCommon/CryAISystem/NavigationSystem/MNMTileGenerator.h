// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryMath/Cry_Geo.h>
#include <CryMath/SimpleHashLookUp.h>

#include <CryAISystem/NavigationSystem/NavigationIdTypes.h>
#include <CryAISystem/NavigationSystem/MNMTile.h>

namespace MNM
{
namespace TileGenerator
{

//! Tile generation parameters.
struct SExtensionParams
{
	::AABB                tileAabbWorld;         //!< Tile's bounding box
	::AABB                extendedTileAabbWorld; //!< Tile's extended bounding box, which includes pieces of neighbors
	NavigationAgentTypeID navAgentTypeId;        //!< Navigation agent type for wich the NavMesh is generated

	// #MNM_TODO pavloi 2016.07.20: expose boundary and exclusion volumes
};

//! IMesh provides an access to the NavMesh which is being generated.
struct IMesh
{
	virtual ~IMesh() {}

	//! Add triangles to the tile's NavMesh.
	//! \param pTrianglesWorld Array of triangles in the world space.
	//! \param count Count of triangles.
	//! \param flags Triangle flags which will be set on all added triangles.
	//! \return true if all triangles were added and there is more space left.
	virtual bool AddTrianglesWorld(const Triangle* pTrianglesWorld, const size_t count, const Tile::STriangle::EFlags flags) = 0;
};

//! Tile generator extension can inject addition navigation data into MNM tile during NavMesh generation.
//! \note Methods of the extension may be called from the NavMesh generation jobs, which are executed on different
//! threads. It's responsibility of extension to implement proper data access synchronization.
struct IExtension
{
	virtual ~IExtension() {}

	//! Generate NavMesh
	//! \param params Parameters of the tile generator.
	//! \param generatedMesh Holds NavMesh being generated.
	//! \return true if the generation should continue.
	virtual bool Generate(const SExtensionParams& params, IMesh& generatedMesh) = 0;

	// #MNM_TODO pavloi 2016.07.21: add mesh post-processing step
	// #MNM_TODO pavloi 2016.07.21: add extra steps for standard voxelizing TileGenerator (for example, voxel painting). Or should it go through different interface?
};

} // namespace TileGenerator
} // namespace MNM
