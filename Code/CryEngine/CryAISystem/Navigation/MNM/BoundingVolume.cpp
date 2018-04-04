// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "BoundingVolume.h"

namespace MNM
{
void BoundingVolume::Swap(BoundingVolume& other)
{
	vertices.swap(other.vertices);
	std::swap(aabb, other.aabb);
	std::swap(height, other.height);
}

bool BoundingVolume::Overlaps(const AABB& _aabb) const
{
	if (!Overlap::AABB_AABB(aabb, _aabb))
		return false;

	const size_t vertexCount = vertices.size();

	for (size_t i = 0; i < vertexCount; ++i)
	{
		const Vec3& v0 = vertices[i];

		if (Overlap::Point_AABB2D(v0, _aabb))
			return true;
	}

	float midz = aabb.min.z + (aabb.max.z - aabb.min.z) * 0.5f;

	if (Contains(Vec3(_aabb.min.x, _aabb.min.y, midz)))
		return true;

	if (Contains(Vec3(_aabb.min.x, _aabb.max.y, midz)))
		return true;

	if (Contains(Vec3(_aabb.max.x, _aabb.max.y, midz)))
		return true;

	if (Contains(Vec3(_aabb.max.x, _aabb.min.y, midz)))
		return true;

	size_t ii = vertexCount - 1;
	for (size_t i = 0; i < vertexCount; ++i)
	{
		const Vec3& v0 = vertices[ii];
		const Vec3& v1 = vertices[i];
		ii = i;

		if (Overlap::Lineseg_AABB2D(Lineseg(v0, v1), _aabb))
			return true;
	}

	return false;
}

BoundingVolume::ExtendedOverlap BoundingVolume::Contains(const AABB& _aabb) const
{
	if (!Overlap::AABB_AABB(aabb, _aabb))
		return NoOverlap;

	const size_t vertexCount = vertices.size();
	size_t inCount = 0;

	if (Contains(Vec3(_aabb.min.x, _aabb.min.y, _aabb.min.z)))
		++inCount;

	if (Contains(Vec3(_aabb.min.x, _aabb.min.y, _aabb.max.z)))
		++inCount;

	if (Contains(Vec3(_aabb.min.x, _aabb.max.y, _aabb.min.z)))
		++inCount;

	if (Contains(Vec3(_aabb.min.x, _aabb.max.y, _aabb.max.z)))
		++inCount;

	if (Contains(Vec3(_aabb.max.x, _aabb.max.y, _aabb.min.z)))
		++inCount;

	if (Contains(Vec3(_aabb.max.x, _aabb.max.y, _aabb.max.z)))
		++inCount;

	if (Contains(Vec3(_aabb.max.x, _aabb.min.y, _aabb.max.z)))
		++inCount;

	if (Contains(Vec3(_aabb.max.x, _aabb.min.y, _aabb.max.z)))
		++inCount;

	if (inCount != 8)
		return PartialOverlap;

	size_t ii = vertexCount - 1;

	for (size_t i = 0; i < vertexCount; ++i)
	{
		const Vec3& v0 = vertices[ii];
		const Vec3& v1 = vertices[i];
		ii = i;

		if (Overlap::Lineseg_AABB2D(Lineseg(v0, v1), _aabb))
			return PartialOverlap;
	}

	return FullOverlap;
}

bool BoundingVolume::Contains(const Vec3& point) const
{
	if (!Overlap::Point_AABB(point, aabb))
		return false;

	const size_t vertexCount = vertices.size();
	bool count = false;

	size_t ii = vertexCount - 1;
	for (size_t i = 0; i < vertexCount; ++i)
	{
		const Vec3& v0 = vertices[ii];
		const Vec3& v1 = vertices[i];
		ii = i;

		if ((((v1.y <= point.y) && (point.y < v0.y)) || ((v0.y <= point.y) && (point.y < v1.y))) &&
		    (point.x < (v0.x - v1.x) * (point.y - v1.y) / (v0.y - v1.y) + v1.x))
		{
			count = !count;
		}
	}

	return count != 0;
}

/*
   bool BoundingVolume::IntersectLineSeg(const Vec3& start, const Vec3& end, float* t, size_t* segment) const
   {
   float bestt = FLT_MAX;
   size_t bestSeg = ~0ul;

   const size_t vertexCount = vertices.size();

   size_t ii = vertexCount - 1;

   for (size_t i = 0; i < vertexCount; ++i)
   {
    const Vec3& v0 = vertices[ii];
    const Vec3& v1 = vertices[i];
    ii = i;

    float a, b;
    if (Intersect::Lineseg_Lineseg2D(Lineseg(start, end), Lineseg(v0, v1), a, b))
    {
      if (a < bestt)
      {
        bestt = a;
        bestSeg = ii;
      }
    }
   }

   if (bestSeg != ~0ul)
   {
    if (t)
 * t = bestt;

    if (segment)
 * segment = bestSeg;

    return true;
   }

   return false;
   }
 */

void BoundingVolume::OffsetVerticesAndAABB(const Vec3& delta)
{
#ifdef SEG_WORLD
	const size_t vertexCount = vertices.size();
	for (size_t i = 0; i < vertexCount; ++i)
	{
		vertices[i] += delta;
	}
	aabb.Move(delta);
#endif
}
}
