// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BoundingVolume_h__
#define __BoundingVolume_h__

#pragma once

#include <CryAISystem/NavigationSystem/MNMBoundingVolume.h>

namespace MNM
{
struct BoundingVolume : MNM::SBoundingVolume
{
	typedef std::vector<Vec3> Boundary;

	const Boundary& GetBoundaryVertices() const { return vertices; }

	void Set(const Vec3* pVertices, const size_t verticesCount, const float height);

	void Swap(BoundingVolume& other);

	bool Overlaps(const AABB& _aabb) const;

	enum ExtendedOverlap
	{
		NoOverlap      = 0,
		PartialOverlap = 1,
		FullOverlap    = 2,
	};

	ExtendedOverlap Contains(const AABB& _aabb) const;
	bool            Contains(const Vec3& point) const;

	bool            IntersectLineSeg(const Vec3& start, const Vec3& end, float* t, size_t* segment) const;

	void            OffsetVerticesAndAABB(const Vec3& delta);

private:
	Boundary vertices;
};

struct HashBoundingVolume
{
	BoundingVolume vol;
	uint32         hash;
};
}

#endif
