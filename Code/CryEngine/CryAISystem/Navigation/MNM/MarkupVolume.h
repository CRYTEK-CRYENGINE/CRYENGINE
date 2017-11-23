// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BoundingVolume.h"

#include <CryAISystem/NavigationSystem/MNMBaseTypes.h>

namespace MNM
{

struct SMarkupVolumeData
{
	struct MeshTriangles
	{
		MeshTriangles(NavigationMeshID meshId) : meshId(meshId) {}
		NavigationMeshID meshId;
		std::vector<TriangleID> triangleIds;
	};
	std::vector<MeshTriangles> meshTriangles;
};

struct SMarkupVolume : public MNM::BoundingVolume
{
	void Swap(SMarkupVolume& other);

	// Properties
	AreaAnnotation areaAnnotation;
	bool bStoreTriangles;
	bool bExpandByAgentRadius;
};

}
