// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef WORLD_COLLISION_H
#define WORLD_COLLISION_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CrySystem/ISystem.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ITimer.h>
#include <CryAISystem/IAgent.h>

#include <limits>

//===================================================================
// CalcPolygonArea
//===================================================================
template<typename VecContainer, typename T>
T CalcPolygonArea(const VecContainer& pts)
{
	if (pts.size() < 2)
		return T(0);

	T totalCross = T(0);
	const typename VecContainer::const_iterator itEnd = pts.end();
	typename VecContainer::const_iterator it = pts.begin();
	typename VecContainer::const_iterator itNext = it;
	for (; it != itEnd; ++it)
	{
		if (++itNext == itEnd)
			itNext = pts.begin();
		totalCross += it->x * itNext->y - itNext->x * it->y;
	}

	return totalCross / T(2);
}

//===================================================================
// IsWoundAnticlockwise
//===================================================================
template<typename VecContainer, typename T>
bool IsWoundAnticlockwise(const VecContainer& pts)
{
	if (pts.size() < 2)
		return true;
	return CalcPolygonArea<VecContainer, T>(pts) > T(0);
}

//===================================================================
// EnsureShapeIsWoundAnticlockwise
//===================================================================
template<typename VecContainer, typename T>
void EnsureShapeIsWoundAnticlockwise(VecContainer& pts)
{
	if (!IsWoundAnticlockwise<VecContainer, T>(pts))
		std::reverse(pts.begin(), pts.end());
}

//===================================================================
// OverlapLinesegAABB2D
//===================================================================
inline bool OverlapLinesegAABB2D(const Vec3& p0, const Vec3& p1, const AABB& aabb)
{
	Vec3 c = (aabb.min + aabb.max) * 0.5f;
	Vec3 e = aabb.max - c;
	Vec3 m = (p0 + p1) * 0.5f;
	Vec3 d = p1 - m;
	m = m - c;

	// Try world coordinate axes as separating axes
	float adx = fabsf(d.x);
	if (fabsf(m.x) > e.x + adx) return false;
	float ady = fabsf(d.y);
	if (fabsf(m.y) > e.y + ady) return false;

	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis (see text for detail)
	const float EPSILON = 0.000001f;
	adx += EPSILON;
	ady += EPSILON;

	if (fabsf(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) return false;

	// No separating axis found; segment must be overlapping AABB
	return true;
}

//====================================================================
// SetAABBCornerPoints
//====================================================================
inline void SetAABBCornerPoints(const AABB& b, Vec3* pts)
{
	pts[0].Set(b.min.x, b.min.y, b.min.z);
	pts[1].Set(b.max.x, b.min.y, b.min.z);
	pts[2].Set(b.max.x, b.max.y, b.min.z);
	pts[3].Set(b.min.x, b.max.y, b.min.z);

	pts[4].Set(b.min.x, b.min.y, b.max.z);
	pts[5].Set(b.max.x, b.min.y, b.max.z);
	pts[6].Set(b.max.x, b.max.y, b.max.z);
	pts[7].Set(b.min.x, b.max.y, b.max.z);
}

#endif

