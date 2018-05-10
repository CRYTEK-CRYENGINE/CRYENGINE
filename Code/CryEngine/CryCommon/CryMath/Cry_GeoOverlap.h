// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:Cry_GeoOverlap.h
//	Description: Common overlap-tests
//
//	History:
//	-March 15,2003: Created by Ivo Herzeg
//  -April  7,2005: Added Point_Polygon2
//
//////////////////////////////////////////////////////////////////////

#ifndef CRYOVERLAP_H
#define CRYOVERLAP_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMath/Cry_Geo.h>

namespace Distance
{
template<typename F> ILINE F Point_TriangleSq(const Vec3_tpl<F>& p, const Triangle_tpl<F>& t);
template<typename F> ILINE F Point_Lineseg2DSq(Vec3_tpl<F> p, Lineseg lineseg, F& fT);
}

namespace Overlap {

ILINE void FINDMINMAX(const f32 x0, const f32 x1, const f32 x2, f32& min_value, f32& max_value)
{
	min_value = max_value = x0;
	min_value = min(x1, min_value);
	max_value = max(x1, max_value);
	min_value = min(x2, min_value);
	max_value = max(x2, max_value);
}

inline bool Lineseg_AABB2D(const Lineseg&, const AABB&);
ILINE bool  AABB_AABB2D(const AABB&, const AABB&);

//! Checks if the point is inside an AABB.
/*	inline bool	Point_AABB(const Vec3 &p, const Vec3 &mins,const Vec3 &maxs)
   {
    if ((p.x>=mins.x && p.x<=maxs.x) && (p.y>=mins.y && p.y<=maxs.y) && (p.z>=mins.z && p.z<=maxs.z))	return (true);
    return (false);
   }*/

//! Checks if the point is inside an AABB.
ILINE bool Point_AABB(const Vec3& p, const AABB& aabb)
{
	return ((p.x >= aabb.min.x && p.x < aabb.max.x) && (p.y >= aabb.min.y && p.y < aabb.max.y) && (p.z >= aabb.min.z && p.z < aabb.max.z));
}

template<typename PtType>
ILINE bool Point_AABB2D(const PtType& p, const AABB& aabb)
{
	return ((p.x >= aabb.min.x && p.x < aabb.max.x) && (p.y >= aabb.min.y && p.y < aabb.max.y));
}

//! Checks if a point is inside an OBB.
//! Example: bool result=Overlap::Point_OBB( point, obb );
ILINE bool Point_OBB(const Vec3& p, const Vec3& wpos, const OBB& obb)
{
	AABB aabb = AABB(obb.c - obb.h, obb.c + obb.h);
	Vec3 t = (p - wpos) * obb.m33;
	return ((t.x >= aabb.min.x && t.x <= aabb.max.x) && (t.y >= aabb.min.y && t.y <= aabb.max.y) && (t.z >= aabb.min.z && t.z <= aabb.max.z));
}

//! Checks if a point is inside a sphere.
//! Example: bool result=Overlap::Point_Sphere( point, sphere );
ILINE bool Point_Sphere(const Vec3& p, const Sphere& s)
{
	Vec3 distc = p - s.center;
	f32 sqrad = s.radius * s.radius;
	return (sqrad > (distc | distc));
}

//! Two line segments, in 2D (ignore z).
inline bool Lineseg_Lineseg2D(const Lineseg& lineA, const Lineseg& lineB)
{
	const float Epsilon = 0.0000001f;

	Vec3 delta = lineB.start - lineA.start;

	Vec3 dirA = lineA.end - lineA.start;
	Vec3 dirB = lineB.end - lineB.start;

	float det = dirA.x * dirB.y - dirA.y * dirB.x;
	float detA = delta.x * dirB.y - delta.y * dirB.x;
	float detB = delta.x * dirA.y - delta.y * dirA.x;

	float absDet = fabs_tpl(det);

	if (absDet >= Epsilon)
	{
		float invDet = 1.0f / det;

		float a = detA * invDet;
		float b = detB * invDet;

		if ((a > 1.0f) || (a < 0.0f) || (b > 1.0f) || (b < 0.0f))
			return false;
	}
	else
		return false;

	return true;
}

//! Check if a point is within a polygon, in 2D (i.e. ignoring z coordinate)
//! The VecContainer should be a container of Vec3_tpl<F> such that we can traverse
//! it using iterators.
//!
//! This works by checking all the lines that start/end above/below the point
//! and counting how many have the point on the right/left side. If the
//! point is inside, then left/right counts should be odd - if outside then
//! even. If it's on the edge then this algorithm will return an arbitrary
//! result. This is basically the same as casting a ray horizontally and
//! counting the intersections.
//! Don't mess with putting tolerances etc in this code, or chaning the <
//! to <= etc in case you're worried about the "ray" going through a vertex.
//! the only issue is when the point is on the edge - if you're worried about
//! that then use the separate function that indicates if a point is within
//! a certain tolerance of an edge.
template<typename PtType, typename VecContainer>
inline bool Point_Polygon2D(const PtType& p, const VecContainer& polygon, const AABB* pAABBPolygon = 0)
{
	if (pAABBPolygon && !Overlap::Point_AABB2D(p, *pAABBPolygon))
		return false;

	int count = 0;

	typename VecContainer::const_iterator li, linext;
	typename VecContainer::const_iterator liend = polygon.end();
	for (li = polygon.begin(); li != liend; ++li)
	{
		linext = li;
		++linext;
		if (linext == liend)
			linext = polygon.begin();
		const PtType& l0 = *li;
		const PtType& l1 = *linext;

		if ((((l1.y <= p.y) && (p.y < l0.y)) ||
		     ((l0.y <= p.y) && (p.y < l1.y))) &&
		    (p.x < (l0.x - l1.x) * (p.y - l1.y) / (l0.y - l1.y) + l1.x))
		{
			count = !count;
		}
	}
	return (0 != count);
}

template<typename PtType>
inline bool Point_Polygon2D(const PtType& p, const PtType* polygon, size_t vertexCount, const AABB* pAABBPolygon = 0)
{
	if (pAABBPolygon && !Overlap::Point_AABB2D(p, *pAABBPolygon))
		return false;

	bool count = false;

	for (size_t i = 0; i < vertexCount; ++i)
	{
		const PtType l0 = polygon[i % vertexCount];
		const PtType l1 = polygon[(i + 1) % vertexCount];

		if ((((l1.y <= p.y) && (p.y < l0.y)) ||
		     ((l0.y <= p.y) && (p.y < l1.y))) &&
		    (p.x < (l0.x - l1.x) * (p.y - l1.y) / (l0.y - l1.y) + l1.x))
		{
			count = !count;
		}
	}

	return count;
}

template<typename F, typename VecContainer>
inline bool Circle_Polygon2D(const Vec3_tpl<F>& p, F radius, const VecContainer& polygon)
{
	if (Overlap::Point_Polygon2D(p, polygon))
		return true;
	typename VecContainer::const_iterator li, linext;
	typename VecContainer::const_iterator liend = polygon.end();
	for (li = polygon.begin(); li != liend; ++li)
	{
		linext = li;
		++linext;
		if (linext == liend)
			linext = polygon.begin();
		const Vec3_tpl<F>& l0 = *li;
		const Vec3_tpl<F>& l1 = *linext;
		float junk;
		if (Distance::Point_Lineseg2DSq(p, Lineseg(l0, l1), junk) < square(radius))
			return true;
	}
	return false;
}
//! Checks if a line segment overlaps a polygon (defined by a container of sorted points) in 2D (ignoring z).
//! If you know the polygon AABB pass it in (z is ignored).
template<typename VecContainer>
inline bool Lineseg_Polygon2D(const Lineseg& lineseg,
                              const VecContainer& polygon,
                              const AABB* pAABBPolygon = 0)
{
	if (pAABBPolygon && !Overlap::Lineseg_AABB2D(lineseg, *pAABBPolygon))
		return false;

	typename VecContainer::const_iterator it;
	for (it = polygon.begin(); it != polygon.end(); ++it)
	{
		typename VecContainer::const_iterator itNext = it;
		++itNext;
		if (itNext == polygon.end())
			itNext = polygon.begin();

		Lineseg polySeg(*it, *itNext);
		if (Lineseg_Lineseg2D(lineseg, polySeg))
			return true;
	}
	return false;
}

//! Checks for overlap of two polygons in 2D (ignoring z).
template<typename VecContainerA, typename VecContainerB>
inline bool Polygon_Polygon2D(const VecContainerA& polygonA, const VecContainerB& polygonB,
                              const AABB* pAABBA = 0, const AABB* pAABBB = 0)
{
	const typename VecContainerA::const_iterator itEndA = polygonA.end();
	const typename VecContainerB::const_iterator itEndB = polygonB.end();
	typename VecContainerA::const_iterator itA;
	typename VecContainerB::const_iterator itB;

	AABB aabbA, aabbB;
	if (!pAABBA)
	{
		pAABBA = &aabbA;
		aabbA.Reset();
		for (itA = polygonA.begin(); itA != itEndA; ++itA)
			aabbA.Add(*itA);
	}
	if (!pAABBB)
	{
		pAABBB = &aabbB;
		aabbB.Reset();
		for (itB = polygonB.begin(); itB != itEndB; ++itB)
			aabbB.Add(*itB);
	}

	// AABB
	if (!Overlap::AABB_AABB2D(*pAABBA, *pAABBB))
		return false;

	// points of A in B
	for (itA = polygonA.begin(); itA != itEndA; ++itA)
	{
		if (Point_Polygon2D(*itA, polygonB, pAABBB))
			return true;
	}
	// points of B in A
	for (itB = polygonB.begin(); itB != itEndB; ++itB)
	{
		if (Point_Polygon2D(*itB, polygonA, pAABBA))
			return true;
	}
	// segments of one A against B
	for (itA = polygonA.begin(); itA != itEndA; ++itA)
	{
		typename VecContainerA::const_iterator itNext = itA;
		++itNext;
		if (itNext == itEndA)
			itNext = polygonA.begin();
		if (Lineseg_Polygon2D(Lineseg(*itA, *itNext), polygonB, pAABBB))
			return true;
	}
	return false;
}

//! Checks if a triangle (defined by three vertices) overlaps a polygon (defined by a container of sorted points), all in 2D only (ignoring z).
template<typename F, typename VecContainer>
inline bool Triangle_Polygon2D(const Vec3_tpl<F>& p0, const Vec3_tpl<F>& p1, const Vec3_tpl<F>& p2,
                               const VecContainer& polygon)
{
	// Check for triangle points within the poly
	if (Point_Polygon2D(p0, polygon))
		return true;
	if (Point_Polygon2D(p1, polygon))
		return true;
	if (Point_Polygon2D(p2, polygon))
		return true;

	// check the poly points against the triangle
	static std::vector<Vec3> triangle;
	triangle.clear();
	triangle.push_back(p0);
	triangle.push_back(p1);
	triangle.push_back(p2);

	typename VecContainer::const_iterator it;
	for (it = polygon.begin(); it != polygon.end(); ++it)
	{
		if (Point_Polygon2D(*it, triangle))
			return true;
	}

	// finally check the edges
	if (Overlap::Lineseg_Polygon2D(Lineseg(p0, p1), polygon))
		return true;
	if (Overlap::Lineseg_Polygon2D(Lineseg(p1, p2), polygon))
		return true;
	if (Overlap::Lineseg_Polygon2D(Lineseg(p2, p0), polygon))
		return true;
	return false;
}

//-----------------------------------------------------------------------------------------

//! check if a Lineseg and a Sphere overlap
inline bool Lineseg_Sphere(const Lineseg& ls, const Sphere& s)
{

	float radius2 = s.radius * s.radius;

	//check if one of the two edpoints of the line is inside the sphere
	Vec3 diff = ls.end - s.center;
	if (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z <= radius2) return true;

	Vec3 AC = s.center - ls.start;
	if (AC.x * AC.x + AC.y * AC.y + AC.z * AC.z <= radius2) return true;

	//check distance from the sphere to the line
	Vec3 AB = ls.end - ls.start;

	float r = (AC.x * AB.x + AC.y * AB.y + AC.z * AB.z) / (AB.x * AB.x + AB.y * AB.y + AB.z * AB.z);

	//projection falls outside the line
	if (r < 0 || r > 1) return false;

	//check if the distance from the line to the center of the sphere is less than radius
	Vec3 point = ls.start + r * AB;
	if ((point.x - s.center.x) * (point.x - s.center.x) + (point.y - s.center.y) * (point.y - s.center.y) + (point.z - s.center.z) * (point.z - s.center.z) > radius2)  return false;

	return true;
}

/*!
 * we use the SEPARATING AXIS TEST to check if a Linesegment overlap an AABB.
 *
 * Example:
 *  bool result=Overlap::Lineseg_AABB( ls, pos,aabb );
 */
inline bool Lineseg_AABB(const Lineseg& ls, const AABB& aabb)
{
	//calculate the half-length-vectors of the AABB
	Vec3 h = (aabb.max - aabb.min) * 0.5f;
	//"t" is the transfer-vector from one center to the other
	Vec3 t = (ls.start + ls.end) * 0.5f - (aabb.max + aabb.min) * 0.5f;
	//calculate line-direction
	Vec3 ld = (ls.end - ls.start) * 0.5f;
	if (fabsf(t.x) > (h.x + fabsf(ld.x))) return 0;
	if (fabsf(t.y) > (h.y + fabsf(ld.y))) return 0;
	if (fabsf(t.z) > (h.z + fabsf(ld.z))) return 0;
	if (fabsf(t.z * ld.y - t.y * ld.z) > (fabsf(h.y * ld.z) + fabsf(h.z * ld.y)))  return 0;
	if (fabsf(t.x * ld.z - t.z * ld.x) > (fabsf(h.x * ld.z) + fabsf(h.z * ld.x)))  return 0;
	if (fabsf(t.y * ld.x - t.x * ld.y) > (fabsf(h.x * ld.y) + fabsf(h.y * ld.x)))  return 0;
	return 1;   //no separating axis found, objects overlap
}

//! 2D seaparating axis test - ignores z.
inline bool Lineseg_AABB2D(const Lineseg& ls, const AABB& aabb)
{
	//calculate the half-length-vectors of the AABB
	Vec3 h = (aabb.max - aabb.min) * 0.5f;
	//"t" is the transfer-vector from one center to the other
	Vec3 t = (ls.start + ls.end) * 0.5f - (aabb.max + aabb.min) * 0.5f;
	//calculate line-direction
	Vec3 ld = (ls.end - ls.start) * 0.5f;
	if (fabsf(t.x) > (h.x + fabsf(ld.x))) return 0;
	if (fabsf(t.y) > (h.y + fabsf(ld.y))) return 0;
	if (fabsf(t.y * ld.x - t.x * ld.y) > (fabsf(h.x * ld.y) + fabsf(h.y * ld.x)))  return 0;
	return 1;   //no separating axis found, objects overlap
}

/*!
 * we use the SEPARATING AXIS TEST to check if two OBB's overlap.
 *
 * Example:
 *  bool result=Overlap::Lineseg_OBB( lineseg, pos,obb );
 */
inline bool Lineseg_OBB(const Lineseg& ls, const Vec3& pos, const OBB& obb)
{
	//the new center-position of Lineseg and OBB in world-space
	Vec3 wposobb = obb.m33 * obb.c + pos;
	Vec3 wposls = (ls.start + ls.end) * 0.5f;
	//"t" is the transfer-vector from one center to the other
	Vec3 t = (wposls - wposobb) * obb.m33;
	//! calculate line-direction in local obb-space
	Vec3 ld = ((ls.end - ls.start) * obb.m33) * 0.5f;
	if (fabsf(t.x) > (obb.h.x + fabsf(ld.x))) return 0;
	if (fabsf(t.y) > (obb.h.y + fabsf(ld.y))) return 0;
	if (fabsf(t.z) > (obb.h.z + fabsf(ld.z))) return 0;
	if (fabsf(t.z * ld.y - t.y * ld.z) > (fabsf(obb.h.y * ld.z) + fabsf(obb.h.z * ld.y))) return 0;
	if (fabsf(t.x * ld.z - t.z * ld.x) > (fabsf(obb.h.x * ld.z) + fabsf(obb.h.z * ld.x))) return 0;
	if (fabsf(t.y * ld.x - t.x * ld.y) > (fabsf(obb.h.x * ld.y) + fabsf(obb.h.y * ld.x))) return 0;
	return 1;   //no separating axis found, objects overlap
}

/*!
 *
 * overlap-test between a line and a triangle.
 * IMPORTANT: this is a single-sided test. That means its not enough
 * that the triangle and line overlap, its also important that the triangle
 * is "visible" when you are looking along the line-direction.
 *
 * If you need a double-sided test, you'll have to call this function twice with
 * reversed order of triangle vertices.
 *
 * return values
 * return "true" if line and triangle overlap.
 *
 */
inline bool Line_Triangle(const Line& line, const Vec3& v0, const Vec3& v1, const Vec3& v2)
{
	const float Epsilon = 0.0000001f;

	Vec3 edgeA = v1 - v0;
	Vec3 edgeB = v2 - v0;

	Vec3 dir = line.direction;

	Vec3 p = dir.Cross(edgeA);
	Vec3 t = line.pointonline - v0;
	Vec3 q = t.Cross(edgeB);

	float dot = edgeB.Dot(p);

	float u = t.Dot(p);
	float v = dir.Dot(q);

	float DotGreaterThanEpsilon = dot - Epsilon;
	float VGreaterEqualThanZero = v;
	float UGreaterEqualThanZero = u;
	float UVLessThanDot = dot - (u + v);
	float ULessThanDot = dot - u;

	float UVGreaterEqualThanZero = (float)__fsel(VGreaterEqualThanZero, UGreaterEqualThanZero, VGreaterEqualThanZero);
	float UUVLessThanDot = (float)__fsel(UVLessThanDot, ULessThanDot, UVLessThanDot);
	float BothGood = (float)__fsel(UVGreaterEqualThanZero, UUVLessThanDot, UVGreaterEqualThanZero);
	float AllGood = (float)__fsel(DotGreaterThanEpsilon, BothGood, DotGreaterThanEpsilon);

	return AllGood >= 0.0f;
}

/*!
 *
 * overlap-test between a ray and a triangle.
 * IMPORTANT: this is a single-sided test. That means its not sufficient
 * that the triangle and ray overlap, its also important that the triangle
 * is "visible" when you are looking from the origin along the ray-direction.
 *
 * If you need a double-sided test, you'll have to call this function twice with
 * reversed order of triangle vertices.
 *
 * return values
 * return "true" if ray and triangle overlap.
 */
inline bool Ray_Triangle(const Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2)
{
	const float Epsilon = 0.0000001f;

	Vec3 edgeA = v1 - v0;
	Vec3 edgeB = v2 - v0;

	Vec3 dir = ray.direction;

	Vec3 p = dir.Cross(edgeA);
	Vec3 t = ray.origin - v0;
	Vec3 q = t.Cross(edgeB);

	float dot = edgeB.Dot(p);

	float u = t.Dot(p);
	float v = dir.Dot(q);

	float DotGreaterThanEpsilon = dot - Epsilon;
	float VGreaterEqualThanZero = v;
	float UGreaterEqualThanZero = u;
	float UVLessThanDot = dot - (u + v);
	float ULessThanDot = dot - u;

	float UVGreaterEqualThanZero = (float)__fsel(VGreaterEqualThanZero, UGreaterEqualThanZero, VGreaterEqualThanZero);
	float UUVLessThanDot = (float)__fsel(UVLessThanDot, ULessThanDot, UVLessThanDot);
	float BothGood = (float)__fsel(UVGreaterEqualThanZero, UUVLessThanDot, UVGreaterEqualThanZero);
	float AllGood = (float)__fsel(DotGreaterThanEpsilon, BothGood, DotGreaterThanEpsilon);

	if (AllGood < 0.0f)
		return false;

	float dt = edgeA.Dot(q) / dot;

	Vec3 result = (dir * dt) + ray.origin;

	float AfterStart = (result - ray.origin).Dot(dir);

	return AfterStart >= 0.0f;
}

/*!
 *
 * overlap-test between line-segment and a triangle.
 * IMPORTANT: this is a single-sided test. That means its not sufficient
 * that the triangle and line-segment overlap, its also important that the triangle
 * is "visible" when you are looking along the linesegment from "start" to "end".
 *
 * If you need a double-sided test, you'll have to call this function twice with
 * reversed order of triangle vertices.
 *
 * return values
 * return "true" if linesegment and triangle overlap.
 */
inline bool Lineseg_Triangle(const Lineseg& lineseg, const Vec3& v0, const Vec3& v1, const Vec3& v2)
{
	const float Epsilon = 0.0000001f;

	Vec3 edgeA = v1 - v0;
	Vec3 edgeB = v2 - v0;

	Vec3 dir = lineseg.end - lineseg.start;

	Vec3 p = dir.Cross(edgeA);
	Vec3 t = lineseg.start - v0;
	Vec3 q = t.Cross(edgeB);

	float dot = edgeB.Dot(p);

	float u = t.Dot(p);
	float v = dir.Dot(q);

	float DotGreaterThanEpsilon = dot - Epsilon;
	float VGreaterEqualThanZero = v;
	float UGreaterEqualThanZero = u;
	float UVLessThanDot = dot - (u + v);
	float ULessThanDot = dot - u;

	float UVGreaterEqualThanZero = (float)__fsel(VGreaterEqualThanZero, UGreaterEqualThanZero, VGreaterEqualThanZero);
	float UUVLessThanDot = (float)__fsel(UVLessThanDot, ULessThanDot, UVLessThanDot);
	float BothGood = (float)__fsel(UVGreaterEqualThanZero, UUVLessThanDot, UVGreaterEqualThanZero);
	float AllGood = (float)__fsel(DotGreaterThanEpsilon, BothGood, DotGreaterThanEpsilon);

	if (AllGood < 0.0f)
		return false;

	float dt = edgeA.Dot(q) / dot;

	Vec3 result = (dir * dt) + lineseg.start;

	float AfterStart = (result - lineseg.start).Dot(dir);
	float BeforeEnd = -(result - lineseg.end).Dot(dir);
	float Within = (float)__fsel(AfterStart, BeforeEnd, AfterStart);

	return Within >= 0.0f;
}

/*----------------------------------------------------------------------------------
 * Sphere_AABB
 *	Sphere and AABB are assumed to be in the same space
 *
 * Example:
 *  bool result=Overlap::Sphere_AABB_Inside( sphere, aabb );
 *
 * 0 = no overlap
 * 1 = overlap
 *----------------------------------------------------------------------------------*/
ILINE bool Sphere_AABB(const Sphere& s, const AABB& aabb)
{
	Vec3 center(s.center);

	Vec3 aabb_min(aabb.min);
	Vec3 aabb_max(aabb.max);

	float radiusSq = s.radius * s.radius;

	float x = (float)__fsel(aabb_min.x - center.x, center.x - aabb_min.x, __fsel(center.x - aabb_max.x, center.x - aabb_max.x, 0.0f));
	float y = (float)__fsel(aabb_min.y - center.y, center.y - aabb_min.y, __fsel(center.y - aabb_max.y, center.y - aabb_max.y, 0.0f));
	float z = (float)__fsel(aabb_min.z - center.z, center.z - aabb_min.z, __fsel(center.z - aabb_max.z, center.z - aabb_max.z, 0.0f));

	return (x * x + y * y + z * z) < radiusSq;
}

//! As Sphere_AABB but ignores z parts.
ILINE bool Sphere_AABB2D(const Sphere& s, const AABB& aabb)
{
	Vec3 center(s.center);

	Vec3 aabb_min(aabb.min);
	Vec3 aabb_max(aabb.max);

	float radiusSq = s.radius * s.radius;

	float x = (float)__fsel(aabb_min.x - center.x, center.x - aabb_min.x, __fsel(center.x - aabb_max.x, center.x - aabb_max.x, 0.0f));
	float y = (float)__fsel(aabb_min.y - center.y, center.y - aabb_min.y, __fsel(center.y - aabb_max.y, center.y - aabb_max.y, 0.0f));

	return (x * x + y * y) < radiusSq;
}

/*!
 *
 * conventional method to check if a Sphere and an AABB overlap,
 * or if the Sphere is completely inside the AABB.
 *	Sphere and AABB are assumed to be in the same space
 *
 * Example:
 *  bool result=Overlap::Sphere_AABB_Inside( sphere, aabb );
 *
 * return values:
 * 0x00 = no overlap
 * 0x01 = Sphere and AABB overlap
 * 0x02 = Sphere in inside AABB
 */
ILINE char Sphere_AABB_Inside(const Sphere& s, const AABB& aabb)
{
	if (Sphere_AABB(s, aabb))
	{
		Vec3 amin = aabb.min - s.center;
		Vec3 amax = aabb.max - s.center;
		if (amin.x >= (-s.radius)) return 1;
		if (amin.y >= (-s.radius)) return 1;
		if (amin.z >= (-s.radius)) return 1;
		if (amax.x <= (+s.radius)) return 1;
		if (amax.y <= (+s.radius)) return 1;
		if (amax.z <= (+s.radius)) return 1;
		//yes, its inside
		return 2;
	}
	return 0;
}

//! VERY IMPORTANT: Sphere is assumed to be in the space of the OBB, otherwise it won't work.
//! \retval 0 no overlap.
//! \retval 1 overlap.
inline bool Sphere_OBB(const Sphere& s, const OBB& obb)
{
	//first we transform the sphere-center into the AABB-space of the OBB
	Vec3 SphereInOBBSpace = s.center * obb.m33;
	//the rest ist the same as the "Overlap::Sphere_AABB" calculation
	float quatradius = s.radius * s.radius;
	Vec3 quat(0, 0, 0);
	AABB aabb = AABB(obb.c - obb.h, obb.c + obb.h);
	if (SphereInOBBSpace.x < aabb.min.x) { quat.x = SphereInOBBSpace.x - aabb.min.x; }
	else if (SphereInOBBSpace.x > aabb.max.x)
	{
		quat.x = SphereInOBBSpace.x - aabb.max.x;
	}
	if (SphereInOBBSpace.y < aabb.min.y) { quat.y = SphereInOBBSpace.y - aabb.min.y; }
	else if (SphereInOBBSpace.y > aabb.max.y)
	{
		quat.y = SphereInOBBSpace.y - aabb.max.y;
	}
	if (SphereInOBBSpace.z < aabb.min.z) { quat.z = SphereInOBBSpace.z - aabb.min.z; }
	else if (SphereInOBBSpace.z > aabb.max.z)
	{
		quat.z = SphereInOBBSpace.z - aabb.max.z;
	}
	return((quat | quat) < quatradius);
}

//! Sphere_Sphere overlap test.
//! \retval 0 no overlap.
//! \retval 1 overlap.
inline bool Sphere_Sphere(const Sphere& s1, const Sphere& s2)
{
	Vec3 distc = s1.center - s2.center;
	f32 sqrad = (s1.radius + s2.radius) * (s1.radius + s2.radius);
	return (sqrad > (distc | distc));
}

//! Sphere_Triangle overlap test.
//! \retval 0 no overlap.
//! \retval 1 overlap.
template<typename F>
ILINE bool Sphere_Triangle(const Sphere& s, const Triangle_tpl<F>& t)
{
	//create a "bouding sphere" around triangle for fast rejection test
	Vec3_tpl<F> middle = (t.v0 + t.v1 + t.v2) * (1 / 3.0f);
	Vec3_tpl<F> ov0 = t.v0 - middle;
	Vec3_tpl<F> ov1 = t.v1 - middle;
	Vec3_tpl<F> ov2 = t.v2 - middle;
	F SqRad0 = (ov0 | ov0);
	F SqRad1 = (ov1 | ov1);
	F SqRad2 = (ov2 | ov2);

	SqRad0 = (F)__fsel(SqRad0 - SqRad1, SqRad0, SqRad1);
	SqRad0 = (F)__fsel(SqRad0 - SqRad2, SqRad0, SqRad2);

	//first simple rejection-test...
	if (Sphere_Sphere(s, Sphere(middle, sqrt_tpl(SqRad0))) == 0) return 0; //overlap not possible
	//...and now the hardcore-test!
	if ((s.radius * s.radius) < Distance::Point_TriangleSq(s.center, t))  return 0;
	return 1;   //sphere and triangle are overlapping
}

/*!
 *
 * we use the SEPARATING-AXIS-TEST for OBB/Plane overlap.
 *
 * Example:
 *  bool result=Overlap::OBB_Plane( pos,obb, plane );
 *
 */
inline bool OBB_Plane(const Vec3& pos, const OBB& obb, const Plane& plane)
{
	//the new center-position in world-space
	Vec3 p = obb.m33 * obb.c + pos;
	//extract the orientation-vectors from the columns of the 3x3 matrix
	//and scale them by the half-lengths
	Vec3 ax = Vec3(obb.m33.m00, obb.m33.m10, obb.m33.m20) * obb.h.x;
	Vec3 ay = Vec3(obb.m33.m01, obb.m33.m11, obb.m33.m21) * obb.h.y;
	Vec3 az = Vec3(obb.m33.m02, obb.m33.m12, obb.m33.m22) * obb.h.z;
	//check OBB against Plane, using the plane-normal as separating axis
	return fabsf(plane | p) < (fabsf(plane.n | ax) + fabsf(plane.n | ay) + fabsf(plane.n | az));
}

/*!
 *
 * we use the SEPARATING AXIS TEST to check if a triangle and AABB overlap.
 *
 * Example:
 *  bool result=Overlap::AABB_Triangle( pos,aabb, tv0,tv1,tv2 );
 *
 */
inline bool AABB_Triangle(const AABB& aabb, const Vec3& tv0, const Vec3& tv1, const Vec3& tv2)
{

	//------ convert AABB into half-length AABB -----------
	Vec3 h = (aabb.max - aabb.min) * 0.5f; //calculate the half-length-vectors
	Vec3 c = (aabb.max + aabb.min) * 0.5f; //the center is relative to the PIVOT

	//move everything so that the boxcenter is in (0,0,0)
	Vec3 v0 = tv0 - c;
	Vec3 v1 = tv1 - c;
	Vec3 v2 = tv2 - c;

	//compute triangle edges
	Vec3 e0 = v1 - v0;
	Vec3 e1 = v2 - v1;
	Vec3 e2 = v0 - v2;

	//--------------------------------------------------------------------------------------
	//    use SEPARATING AXIS THEOREM to test overlap  between AABB and triangle
	//    cross-product(edge from triangle, {x,y,z}-direction),  this are 3x3=9 tests
	//--------------------------------------------------------------------------------------
	float min, max, p0, p1, p2, rad, fex, fey, fez;
	fex = fabsf(e0.x);
	fey = fabsf(e0.y);
	fez = fabsf(e0.z);

	//AXISTEST_X01(e0.z, e0.y, fez, fey);
	p0 = e0.z * v0.y - e0.y * v0.z;
	p2 = e0.z * v2.y - e0.y * v2.z;
	if (p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }
	rad = fez * h.y + fey * h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Y02(e0.z, e0.x, fez, fex);
	p0 = -e0.z * v0.x + e0.x * v0.z;
	p2 = -e0.z * v2.x + e0.x * v2.z;
	if (p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }
	rad = fez * h.x + fex * h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Z12(e0.y, e0.x, fey, fex);
	p1 = e0.y * v1.x - e0.x * v1.y;
	p2 = e0.y * v2.x - e0.x * v2.y;
	if (p2 < p1) { min = p2; max = p1; } else { min = p1; max = p2; }
	rad = fey * h.x + fex * h.y;
	if (min > rad || max < -rad) return 0;

	//-----------------------------------------------

	fex = fabsf(e1.x);
	fey = fabsf(e1.y);
	fez = fabsf(e1.z);
	//AXISTEST_X01(e1.z, e1.y, fez, fey);
	p0 = e1.z * v0.y - e1.y * v0.z;
	p2 = e1.z * v2.y - e1.y * v2.z;
	if (p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }
	rad = fez * h.y + fey * h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Y02(e1.z, e1.x, fez, fex);
	p0 = -e1.z * v0.x + e1.x * v0.z;
	p2 = -e1.z * v2.x + e1.x * v2.z;
	if (p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }
	rad = fez * h.x + fex * h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Z0(e1.y, e1.x, fey, fex);
	p0 = e1.y * v0.x - e1.x * v0.y;
	p1 = e1.y * v1.x - e1.x * v1.y;
	if (p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; }
	rad = fey * h.x + fex * h.y;
	if (min > rad || max < -rad) return 0;

	//-----------------------------------------------

	fex = fabsf(e2.x);
	fey = fabsf(e2.y);
	fez = fabsf(e2.z);
	//AXISTEST_X2(e2.z, e2.y, fez, fey);
	p0 = e2.z * v0.y - e2.y * v0.z;
	p1 = e2.z * v1.y - e2.y * v1.z;
	if (p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; }
	rad = fez * h.y + fey * h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Y1(e2.z, e2.x, fez, fex);
	p0 = -e2.z * v0.x + e2.x * v0.z;
	p1 = -e2.z * v1.x + e2.x * v1.z;
	if (p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; }
	rad = fez * h.x + fex * h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Z12(e2.y, e2.x, fey, fex);
	p1 = e2.y * v1.x - e2.x * v1.y;
	p2 = e2.y * v2.x - e2.x * v2.y;
	if (p2 < p1) { min = p2; max = p1; } else { min = p1; max = p2; }
	rad = fey * h.x + fex * h.y;
	if (min > rad || max < -rad) return 0;

	//the {x,y,z}-directions (actually, since we use the AABB of the triangle we don't even need to test these)
	//first test overlap in the {x,y,z}-directions
	//find min, max of the triangle each direction, and test for overlap in that direction --
	//this is equivalent to testing a minimal AABB around the triangle against the AABB
	AABB taabb;

	//test in X-direction
	FINDMINMAX(v0.x, v1.x, v2.x, taabb.min.x, taabb.max.x);
	if (taabb.min.x > h.x || taabb.max.x < -h.x) return 0;

	//test in Y-direction
	FINDMINMAX(v0.y, v1.y, v2.y, taabb.min.y, taabb.max.y);
	if (taabb.min.y > h.y || taabb.max.y < -h.y) return 0;

	//test in Z-direction
	FINDMINMAX(v0.z, v1.z, v2.z, taabb.min.z, taabb.max.z);
	if (taabb.min.z > h.z || taabb.max.z < -h.z) return 0;

	//test if the box intersects the plane of the triangle
	//compute plane equation of triangle: normal*x+d=0
	Plane plane = Plane::CreatePlane((e0 % e1), v0);

	Vec3 vmin, vmax;
	if (plane.n.x > 0.0f) { vmin.x = -h.x; vmax.x = +h.x; }
	else { vmin.x = +h.x; vmax.x = -h.x; }
	if (plane.n.y > 0.0f) { vmin.y = -h.y; vmax.y = +h.y; }
	else { vmin.y = +h.y; vmax.y = -h.y; }
	if (plane.n.z > 0.0f) { vmin.z = -h.z; vmax.z = +h.z; }
	else { vmin.z = +h.z;  vmax.z = -h.z;  }
	if ((plane | vmin) > 0.0f) return 0;
	if ((plane | vmax) < 0.0f) return 0;
	return 1;
}

/*!
 *
 * we use the SEPARATING AXIS TEST to check if a triangle and an OBB overlaps.
 *
 * Example:
 *  bool result=Overlap::OBB_Trinagle( pos1,obb1, tv0,tv1,tv2 );
 *
 */
inline bool OBB_Triangle(const Vec3& pos, const OBB& obb, const Vec3& tv0, const Vec3& tv1, const Vec3& tv2)
{

	Vec3 p = obb.m33 * obb.c + pos;                   //the new center-position in world-space

	//move everything so that the boxcenter is in (0,0,0)
	Vec3 v0 = (tv0 - p) * obb.m33; //pre-transform
	Vec3 v1 = (tv1 - p) * obb.m33; //pre-transform
	Vec3 v2 = (tv2 - p) * obb.m33; //pre-transform

	//compute triangle edges
	Vec3 e0 = v1 - v0;
	Vec3 e1 = v2 - v1;
	Vec3 e2 = v0 - v2;

	//--------------------------------------------------------------------------------------
	//    use SEPARATING AXIS THEOREM to test intersection between AABB and triangle
	//    cross-product(edge from triangle, {x,y,z}-direction),  this are 3x3=9 tests
	//--------------------------------------------------------------------------------------
	float min, max, p0, p1, p2, rad, fex, fey, fez;
	fex = fabsf(e0.x);
	fey = fabsf(e0.y);
	fez = fabsf(e0.z);

	//AXISTEST_X01(e0.z, e0.y, fez, fey);
	p0 = e0.z * v0.y - e0.y * v0.z;
	p2 = e0.z * v2.y - e0.y * v2.z;
	if (p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }
	rad = fez * obb.h.y + fey * obb.h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Y02(e0.z, e0.x, fez, fex);
	p0 = -e0.z * v0.x + e0.x * v0.z;
	p2 = -e0.z * v2.x + e0.x * v2.z;
	if (p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }
	rad = fez * obb.h.x + fex * obb.h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Z12(e0.y, e0.x, fey, fex);
	p1 = e0.y * v1.x - e0.x * v1.y;
	p2 = e0.y * v2.x - e0.x * v2.y;
	if (p2 < p1) { min = p2; max = p1; } else { min = p1; max = p2; }
	rad = fey * obb.h.x + fex * obb.h.y;
	if (min > rad || max < -rad) return 0;

	//-----------------------------------------------

	fex = fabsf(e1.x);
	fey = fabsf(e1.y);
	fez = fabsf(e1.z);
	p0 = e1.z * v0.y - e1.y * v0.z;
	p2 = e1.z * v2.y - e1.y * v2.z;
	if (p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }
	rad = fez * obb.h.y + fey * obb.h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Y02(e1.z, e1.x, fez, fex);
	p0 = -e1.z * v0.x + e1.x * v0.z;
	p2 = -e1.z * v2.x + e1.x * v2.z;
	if (p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }
	rad = fez * obb.h.x + fex * obb.h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Z0(e1.y, e1.x, fey, fex);
	p0 = e1.y * v0.x - e1.x * v0.y;
	p1 = e1.y * v1.x - e1.x * v1.y;
	if (p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; }
	rad = fey * obb.h.x + fex * obb.h.y;
	if (min > rad || max < -rad) return 0;

	//-----------------------------------------------

	fex = fabsf(e2.x);
	fey = fabsf(e2.y);
	fez = fabsf(e2.z);
	//AXISTEST_X2(e2.z, e2.y, fez, fey);
	p0 = e2.z * v0.y - e2.y * v0.z;
	p1 = e2.z * v1.y - e2.y * v1.z;
	if (p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; }
	rad = fez * obb.h.y + fey * obb.h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Y1(e2.z, e2.x, fez, fex);
	p0 = -e2.z * v0.x + e2.x * v0.z;
	p1 = -e2.z * v1.x + e2.x * v1.z;
	if (p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; }
	rad = fez * obb.h.x + fex * obb.h.z;
	if (min > rad || max < -rad) return 0;

	//AXISTEST_Z12(e2.y, e2.x, fey, fex);
	p1 = e2.y * v1.x - e2.x * v1.y;
	p2 = e2.y * v2.x - e2.x * v2.y;
	if (p2 < p1) { min = p2; max = p1; } else { min = p1; max = p2; }
	rad = fey * obb.h.x + fex * obb.h.y;
	if (min > rad || max < -rad) return 0;

	//the {x,y,z}-directions (actually, since we use the AABB of the triangle we don't even need to test these)
	//first test overlap in the {x,y,z}-directions
	//find min, max of the triangle each direction, and test for overlap in that direction --
	//this is equivalent to testing a minimal AABB around the triangle against the AABB
	AABB taabb;

	// test in X-direction
	FINDMINMAX(v0.x, v1.x, v2.x, taabb.min.x, taabb.max.x);
	if (taabb.min.x > obb.h.x || taabb.max.x < -obb.h.x) return 0;

	// test in Y-direction
	FINDMINMAX(v0.y, v1.y, v2.y, taabb.min.y, taabb.max.y);
	if (taabb.min.y > obb.h.y || taabb.max.y < -obb.h.y) return 0;

	// test in Z-direction
	FINDMINMAX(v0.z, v1.z, v2.z, taabb.min.z, taabb.max.z);
	if (taabb.min.z > obb.h.z || taabb.max.z < -obb.h.z) return 0;

	//test if the box overlaps the plane of the triangle
	//compute plane equation of triangle: normal*x+d=0
	Plane plane = Plane::CreatePlane((e0 % e1), v0);

	Vec3 vmin, vmax;
	if (plane.n.x > 0.0f) { vmin.x = -obb.h.x; vmax.x = +obb.h.x; }
	else { vmin.x = +obb.h.x; vmax.x = -obb.h.x; }
	if (plane.n.y > 0.0f) { vmin.y = -obb.h.y; vmax.y = +obb.h.y; }
	else { vmin.y = +obb.h.y; vmax.y = -obb.h.y; }
	if (plane.n.z > 0.0f) { vmin.z = -obb.h.z; vmax.z = +obb.h.z; }
	else { vmin.z = +obb.h.z;  vmax.z = -obb.h.z;  }
	if ((plane | vmin) > 0.0f) return 0;
	if ((plane | vmax) < 0.0f) return 0;
	return 1;
}

/*!
 *
 * conventional method to check if two AABB's overlap.
 * both AABBs are assumed to be in the same space
 *
 * Example:
 *  bool result=Overlap::AABB_AABB( aabb1, aabb2 );
 *
 */
ILINE bool AABB_AABB(const AABB& aabb1, const AABB& aabb2)
{
	if (aabb1.min.x >= aabb2.max.x) return 0;
	if (aabb1.min.y >= aabb2.max.y) return 0;
	if (aabb1.min.z >= aabb2.max.z) return 0;
	if (aabb1.max.x <= aabb2.min.x) return 0;
	if (aabb1.max.y <= aabb2.min.y) return 0;
	if (aabb1.max.z <= aabb2.min.z) return 0;
	return 1;   //the aabb's overlap
}

//! AABB overlap test - ignores z values.
ILINE bool AABB_AABB2D(const AABB& aabb1, const AABB& aabb2)
{
	if (aabb1.min.x >= aabb2.max.x) return 0;
	if (aabb1.min.y >= aabb2.max.y) return 0;
	if (aabb1.max.x <= aabb2.min.x) return 0;
	if (aabb1.max.y <= aabb2.min.y) return 0;
	return 1;   //the aabb's overlap
}

/*!
 *
 * Conventional method to check if two AABB's overlap.
 * Both AABBs are in local object space. Used the position-vector
 * to translate them into world-space
 *
 * Example:
 *  bool result=Overlap::AABB_AABB( pos1,aabb1, pos2,aabb2 );
 *
 */
ILINE bool AABB_AABB(const Vec3& pos1, const AABB& aabb1, const Vec3& pos2, const AABB& aabb2)
{
	AABB waabb1(aabb1.min + pos1, aabb1.max + pos1);
	AABB waabb2(aabb2.min + pos2, aabb2.max + pos2);
	return AABB_AABB(waabb1, waabb2);
}

/*!
 *
 * conventional method to check if two AABB's overlap
 * or if AABB1 is comletely inside AABB2.
 * both AABBs are assumed to be in the same space
 *
 * Example:
 *  bool result=Overlap::AABB_AABB_Inside( aabb1, aabb2 );
 *
 * return values:
 * 0x00 = no overlap
 * 0x01 = both AABBs one overlap
 * 0x02 = AABB1 in inside AABB2
 */
ILINE char AABB_AABB_Inside(const AABB& aabb1, const AABB& aabb2)
{
	if (AABB_AABB(aabb1, aabb2))
	{
		if (aabb1.min.x <= aabb2.min.x) return 1;
		if (aabb1.min.y <= aabb2.min.y) return 1;
		if (aabb1.min.z <= aabb2.min.z) return 1;
		if (aabb1.max.x >= aabb2.max.x) return 1;
		if (aabb1.max.y >= aabb2.max.y) return 1;
		if (aabb1.max.z >= aabb2.max.z) return 1;
		//yes, its inside
		return 2;
	}
	return 0;
}

/*!
 *
 * we use the SEPARATING AXIS TEST to check if and AABB and an OBB overlap.
 *
 * Example:
 *  bool result=Overlap::AABB_OBB( aabb, pos,obb );
 *
 */
inline bool AABB_OBB(const AABB& aabb, const Vec3& pos, const OBB& obb)
{

	Vec3 h = (aabb.max - aabb.min) * 0.5f; //calculate the half-length-vectors
	Vec3 c = (aabb.max + aabb.min) * 0.5f; //the center in world-space

	//"t" is the transfer-vector from one center to the other
	Vec3 t = obb.m33 * obb.c + pos - c;

	f32 ra, rb;

	//--------------------------------------------------------------------------
	//--  we use the vectors "1,0,0","0,1,0" and "0,0,1" as separating axis
	//--------------------------------------------------------------------------
	rb = fabsf(obb.m33.m00 * obb.h.x) + fabsf(obb.m33.m01 * obb.h.y) + fabsf(obb.m33.m02 * obb.h.z);
	if (fabsf(t.x) > (fabsf(h.x) + rb)) return 0;
	rb = fabsf(obb.m33.m10 * obb.h.x) + fabsf(obb.m33.m11 * obb.h.y) + fabsf(obb.m33.m12 * obb.h.z);
	if (fabsf(t.y) > (fabsf(h.y) + rb)) return 0;
	rb = fabsf(obb.m33.m20 * obb.h.x) + fabsf(obb.m33.m21 * obb.h.y) + fabsf(obb.m33.m22 * obb.h.z);
	if (fabsf(t.z) > (fabsf(h.z) + rb)) return 0;

	//--------------------------------------------------------------------------
	//--  we use the orientation-vectors "Mx","My" and "Mz" as separating axis
	//--------------------------------------------------------------------------
	ra = fabsf(obb.m33.m00 * h.x) + fabsf(obb.m33.m10 * h.y) + fabsf(obb.m33.m20 * h.z);
	if (fabsf(t | Vec3(obb.m33.m00, obb.m33.m10, obb.m33.m20)) > (ra + obb.h.x)) return 0;
	ra = fabsf(obb.m33.m01 * h.x) + fabsf(obb.m33.m11 * h.y) + fabsf(obb.m33.m21 * h.z);
	if (fabsf(t | Vec3(obb.m33.m01, obb.m33.m11, obb.m33.m21)) > (ra + obb.h.y)) return 0;
	ra = fabsf(obb.m33.m02 * h.x) + fabsf(obb.m33.m12 * h.y) + fabsf(obb.m33.m22 * h.z);
	if (fabsf(t | Vec3(obb.m33.m02, obb.m33.m12, obb.m33.m22)) > (ra + obb.h.z)) return 0;

	//---------------------------------------------------------------------
	//----  using 9 cross products we generate new separating axis
	//---------------------------------------------------------------------
	const float e0 = h.x + h.y + h.z, e1 = obb.h.x + obb.h.y + obb.h.z, e = (e0 + e1 - fabsf(e0 - e1)) * 0.0001f;
	ra = h.y * fabsf(obb.m33.m20) + h.z * fabsf(obb.m33.m10);
	rb = obb.h.y * fabsf(obb.m33.m02) + obb.h.z * fabsf(obb.m33.m01);
	if (fabsf(t.z * obb.m33.m10 - t.y * obb.m33.m20) > (ra + rb) + e) return 0;
	ra = h.y * fabsf(obb.m33.m21) + h.z * fabsf(obb.m33.m11);
	rb = obb.h.x * fabsf(obb.m33.m02) + obb.h.z * fabsf(obb.m33.m00);
	if (fabsf(t.z * obb.m33.m11 - t.y * obb.m33.m21) > (ra + rb) + e) return 0;
	ra = h.y * fabsf(obb.m33.m22) + h.z * fabsf(obb.m33.m12);
	rb = obb.h.x * fabsf(obb.m33.m01) + obb.h.y * fabsf(obb.m33.m00);
	if (fabsf(t.z * obb.m33.m12 - t.y * obb.m33.m22) > (ra + rb) + e) return 0;

	ra = h.x * fabsf(obb.m33.m20) + h.z * fabsf(obb.m33.m00);
	rb = obb.h.y * fabsf(obb.m33.m12) + obb.h.z * fabsf(obb.m33.m11);
	if (fabsf(t.x * obb.m33.m20 - t.z * obb.m33.m00) > (ra + rb) + e) return 0;
	ra = h.x * fabsf(obb.m33.m21) + h.z * fabsf(obb.m33.m01);
	rb = obb.h.x * fabsf(obb.m33.m12) + obb.h.z * fabsf(obb.m33.m10);
	if (fabsf(t.x * obb.m33.m21 - t.z * obb.m33.m01) > (ra + rb) + e) return 0;
	ra = h.x * fabsf(obb.m33.m22) + h.z * fabsf(obb.m33.m02);
	rb = obb.h.x * fabsf(obb.m33.m11) + obb.h.y * fabsf(obb.m33.m10);
	if (fabsf(t.x * obb.m33.m22 - t.z * obb.m33.m02) > (ra + rb) + e) return 0;

	ra = h.x * fabsf(obb.m33.m10) + h.y * fabsf(obb.m33.m00);
	rb = obb.h.y * fabsf(obb.m33.m22) + obb.h.z * fabsf(obb.m33.m21);
	if (fabsf(t.y * obb.m33.m00 - t.x * obb.m33.m10) > (ra + rb) + e) return 0;
	ra = h.x * fabsf(obb.m33.m11) + h.y * fabsf(obb.m33.m01);
	rb = obb.h.x * fabsf(obb.m33.m22) + obb.h.z * fabsf(obb.m33.m20);
	if (fabsf(t.y * obb.m33.m01 - t.x * obb.m33.m11) > (ra + rb) + e) return 0;
	ra = h.x * fabsf(obb.m33.m12) + h.y * fabsf(obb.m33.m02);
	rb = obb.h.x * fabsf(obb.m33.m21) + obb.h.y * fabsf(obb.m33.m20);
	if (fabsf(t.y * obb.m33.m02 - t.x * obb.m33.m12) > (ra + rb) + e) return 0;

	//no separating axis found, we have an intersection
	return 1;
}

/*!
 *
 * we use the SEPARATING AXIS TEST to check if two OBB's overlap.
 *
 * Example:
 *  bool result=Overlap::OBB_OBB( pos1,obb1, pos2,obb2 );
 *
 */
inline bool OBB_OBB(const Vec3& pos1, const OBB& obb1, const Vec3& pos2, const OBB& obb2)
{

	//tranform obb2 in local space of obb1
	Matrix33 M = obb1.m33.T() * obb2.m33;

	Vec3 p1 = obb1.m33 * obb1.c + pos1;
	Vec3 p2 = obb2.m33 * obb2.c + pos2;

	//"t" is the transfer-vector from one center to the other
	Vec3 t = (p2 - p1) * obb1.m33;

	float ra, rb;

	//--------------------------------------------------------------------------
	//--  we use the vectors "1,0,0","0,1,0" and "0,0,1" as separating axis
	//--------------------------------------------------------------------------
	rb = fabsf(M.m00 * obb2.h.x) + fabsf(M.m01 * obb2.h.y) + fabsf(M.m02 * obb2.h.z);
	if (fabsf(t.x) > (fabsf(obb1.h.x) + rb)) return 0;
	rb = fabsf(M.m10 * obb2.h.x) + fabsf(M.m11 * obb2.h.y) + fabsf(M.m12 * obb2.h.z);
	if (fabsf(t.y) > (fabsf(obb1.h.y) + rb)) return 0;
	rb = fabsf(M.m20 * obb2.h.x) + fabsf(M.m21 * obb2.h.y) + fabsf(M.m22 * obb2.h.z);
	if (fabsf(t.z) > (fabsf(obb1.h.z) + rb)) return 0;

	//--------------------------------------------------------------------------
	//--  we use the orientation-vectors "Mx","My" and "Mz" as separating axis
	//--------------------------------------------------------------------------
	ra = fabsf(M.m00 * obb1.h.x) + fabsf(M.m10 * obb1.h.y) + fabsf(M.m20 * obb1.h.z);
	if (fabsf(t | Vec3(M.m00, M.m10, M.m20)) > (ra + obb2.h.x)) return 0;
	ra = fabsf(M.m01 * obb1.h.x) + fabsf(M.m11 * obb1.h.y) + fabsf(M.m21 * obb1.h.z);
	if (fabsf(t | Vec3(M.m01, M.m11, M.m21)) > (ra + obb2.h.y)) return 0;
	ra = fabsf(M.m02 * obb1.h.x) + fabsf(M.m12 * obb1.h.y) + fabsf(M.m22 * obb1.h.z);
	if (fabsf(t | Vec3(M.m02, M.m12, M.m22)) > (ra + obb2.h.z)) return 0;

	//---------------------------------------------------------------------
	//----  using 9 cross products we generate new separating axis
	//---------------------------------------------------------------------
	ra = obb1.h.y * fabsf(M.m20) + obb1.h.z * fabsf(M.m10);
	rb = obb2.h.y * fabsf(M.m02) + obb2.h.z * fabsf(M.m01);
	if (fabsf(t.z * M.m10 - t.y * M.m20) > (ra + rb)) return 0;
	ra = obb1.h.y * fabsf(M.m21) + obb1.h.z * fabsf(M.m11);
	rb = obb2.h.x * fabsf(M.m02) + obb2.h.z * fabsf(M.m00);
	if (fabsf(t.z * M.m11 - t.y * M.m21) > (ra + rb)) return 0;
	ra = obb1.h.y * fabsf(M.m22) + obb1.h.z * fabsf(M.m12);
	rb = obb2.h.x * fabsf(M.m01) + obb2.h.y * fabsf(M.m00);
	if (fabsf(t.z * M.m12 - t.y * M.m22) > (ra + rb)) return 0;

	ra = obb1.h.x * fabsf(M.m20) + obb1.h.z * fabsf(M.m00);
	rb = obb2.h.y * fabsf(M.m12) + obb2.h.z * fabsf(M.m11);
	if (fabsf(t.x * M.m20 - t.z * M.m00) > (ra + rb)) return 0;
	ra = obb1.h.x * fabsf(M.m21) + obb1.h.z * fabsf(M.m01);
	rb = obb2.h.x * fabsf(M.m12) + obb2.h.z * fabsf(M.m10);
	if (fabsf(t.x * M.m21 - t.z * M.m01) > (ra + rb)) return 0;
	ra = obb1.h.x * fabsf(M.m22) + obb1.h.z * fabsf(M.m02);
	rb = obb2.h.x * fabsf(M.m11) + obb2.h.y * fabsf(M.m10);
	if (fabsf(t.x * M.m22 - t.z * M.m02) > (ra + rb)) return 0;

	ra = obb1.h.x * fabsf(M.m10) + obb1.h.y * fabsf(M.m00);
	rb = obb2.h.y * fabsf(M.m22) + obb2.h.z * fabsf(M.m21);
	if (fabsf(t.y * M.m00 - t.x * M.m10) > (ra + rb)) return 0;
	ra = obb1.h.x * fabsf(M.m11) + obb1.h.y * fabsf(M.m01);
	rb = obb2.h.x * fabsf(M.m22) + obb2.h.z * fabsf(M.m20);
	if (fabsf(t.y * M.m01 - t.x * M.m11) > (ra + rb)) return 0;
	ra = obb1.h.x * fabsf(M.m12) + obb1.h.y * fabsf(M.m02);
	rb = obb2.h.x * fabsf(M.m21) + obb2.h.y * fabsf(M.m20);
	if (fabsf(t.y * M.m02 - t.x * M.m12) > (ra + rb)) return 0;

	return 1;   //no separating axis found, we have an overlap
}

#define PLANE_X         0
#define PLANE_Y         1
#define PLANE_Z         2
#define PLANE_NON_AXIAL 3

//! check if the point is inside a triangle
inline bool PointInTriangle(const Vec3& point, const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& normal)
{

	float xt, yt;
	Vec3 nn;

	int p1, p2;

	nn = normal;
	nn.x = (float)fabs(nn.x);
	nn.y = (float)fabs(nn.y);
	nn.z = (float)fabs(nn.z);

	if ((nn.x >= nn.y) && (nn.x >= nn.z))
	{
		xt = point.y;
		yt = point.z;
		p1 = PLANE_Y;
		p2 = PLANE_Z;
	}
	else if ((nn.y >= nn.x) && (nn.y >= nn.z))
	{
		xt = point.x;
		yt = point.z;
		p1 = PLANE_X;
		p2 = PLANE_Z;
	}
	else
	{
		xt = point.x;
		yt = point.y;
		p1 = PLANE_X;
		p2 = PLANE_Y;
	}

	float Ax, Ay, Bx, By;
	float s;

	bool front = false;
	bool back = false;

	Ax = (v0)[p1];
	Bx = (v1)[p1];
	Ay = (v0)[p2];
	By = (v1)[p2];

	s = ((Ay - yt) * (Bx - Ax) - (Ax - xt) * (By - Ay));

	if (s >= 0)
	{
		if (back)
			return (false);
		front = true;
	}
	else
	{
		if (front)
			return (false);
		back = true;
	}

	Ax = (v1)[p1];
	Bx = (v2)[p1];
	Ay = (v1)[p2];
	By = (v2)[p2];

	s = ((Ay - yt) * (Bx - Ax) - (Ax - xt) * (By - Ay));

	if (s >= 0)
	{
		if (back)
			return (false);
		front = true;
	}
	else
	{
		if (front) return (false);
		back = true;
	}

	Ax = (v2)[p1];
	Bx = (v0)[p1];
	Ay = (v2)[p2];
	By = (v0)[p2];

	s = ((Ay - yt) * (Bx - Ax) - (Ax - xt) * (By - Ay));

	if (s >= 0)
	{
		if (back)
			return (false);
		front = true;
	}
	else
	{
		if (front)
			return (false);
		back = true;
	}

	return (true);
}

}

#endif //overlap
