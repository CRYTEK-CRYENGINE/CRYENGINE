// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_Vector3.h>

namespace MNM
{

//! Represents the shape of the navigation mesh bounding volume or an exclusion volume
struct SBoundingVolume
{
	AABB        aabb = AABB::RESET;
	const Vec3* pVertices = nullptr;
	size_t      verticesCount = 0;
	float       height = 0.0f;
};

} // namespace MNM
