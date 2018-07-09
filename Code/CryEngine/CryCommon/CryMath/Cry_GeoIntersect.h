// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:Cry_GeoIntersect.h
//	Description: Common intersection-tests
//
//	History:
//	-March 15,2003: Created by Ivo Herzeg
//  -April  7,2005: Added Lineseg_Lineseg2D Lineseg_Polygon2D
//
//////////////////////////////////////////////////////////////////////

#ifndef CRYINTERSECTION_H
#define CRYINTERSECTION_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMath/Cry_Geo.h>

namespace Intersect {

inline bool Ray_Plane(const Ray& ray, const Plane& plane, Vec3& output, bool bSingleSidePlane = true)
{
	float cosine = plane.n | ray.direction;

	//REJECTION 1: if "line-direction" is perpendicular to "plane-normal", an intersection is not possible! That means ray is parallel
	//             to the plane
	//REJECTION 2: if bSingleSidePlane == true we deal with single-sided planes. That means
	//             if "line-direction" is pointing in the same direction as "the plane-normal",
	//             an intersection is not possible!
	if ((cosine == 0.0f) ||                     // normal is orthogonal to vector, cant intersect
	    (bSingleSidePlane && (cosine > 0.0f)))  // we are trying to find an intersection in the same direction as the plane normal
		return false;

	float numer = plane.DistFromPlane(ray.origin);
	float fLength = -numer / cosine;
	output = ray.origin + (ray.direction * fLength);
	//skip, if cutting-point is "behind" ray.origin
	if (fLength < 0.0f)
		return false;

	return true;      //intersection occurred
}

inline bool Line_Plane(const Line& line, const Plane& plane, Vec3& output, bool bSingleSidePlane = true)
{
	float cosine = plane.n | line.direction;

	//REJECTION 1: if "line-direction" is perpendicular to "plane-normal", an intersection is not possible! That means ray is parallel
	//             to the plane
	//REJECTION 2: if bSingleSidePlane == true we deal with single-sided planes. That means
	//             if "line-direction" is pointing in the same direction as "the plane-normal",
	//             an intersection is not possible!
	if ((cosine == 0.0f) ||                     // normal is orthogonal to vector, cant intersect
	    (bSingleSidePlane && (cosine > 0.0f)))  // we are trying to find an intersection in the same direction as the plane normal
		return false;

	//an intersection is possible: calculate the exact point!
	float perpdist = plane | line.pointonline;
	float pd_c = -perpdist / cosine;
	output = line.pointonline + (line.direction * pd_c);

	return true;      //intersection occurred
}

//! Algorithm description: http://softsurfer.com/Archive/algorithm_0104/algorithm_0104B.htm#Line-Plane%20Intersection
template<typename T, typename P>
inline bool Segment_Plane(const Lineseg_tpl<T>& segment, const P& plane, Vec3_tpl<T>& vOutput, bool bSingleSidePlane = true)
{
	Vec3_tpl<T> vSegment = segment.end - segment.start;
	T planeNormalDotSegment = plane.n | vSegment;

	//REJECTION 1: if "line-direction" is perpendicular to "plane-normal", an intersection is not possible! That means ray is parallel
	//             to the plane
	//REJECTION 2: if bSingleSidePlane == true we deal with single-sided planes. That means
	//             if "line-direction" is pointing in the same direction as "the plane-normal",
	//             an intersection is not possible!
	if ((planeNormalDotSegment == T(0)) ||                     // normal is orthogonal to vector, cant intersect
	    (bSingleSidePlane && (planeNormalDotSegment > T(0))))  // we are trying to find an intersection in the same direction as the plane normal
		return false;

	// n Dot (segment.start - closest_point_in_plane) = 1 * DistFromPlane(segment.start) * cos(0) = DistFromPlane(segment.start)
	T distanceToStart = plane.DistFromPlane(segment.start);
	T scale = -distanceToStart / planeNormalDotSegment;
	vOutput = segment.start + (vSegment * scale);

	// skip, if segment start and ends in one side of the plane
	if ((scale < T(0)) || (scale > T(1)))
		return false;

	return true;      //intersection occurred
}

//! Intersection between two line segments in 2D (ignoring z coordinate).
//! The two parametric values are set to between 0 and 1 if intersection occurs. If intersection does
//! not occur their values will indicate the parametric values for intersection of the lines extended
//! beyond the segment lengths. Parallel lines will result in a negative result, but the parametric
//! values will both be equal to 0.5
template<typename F>
inline bool Lineseg_Lineseg2D(const Lineseg_tpl<F>& lineA, const Lineseg_tpl<F>& lineB, F& outA, F& outB)
{
	const F Epsilon = (F)0.0000001;

	Vec3_tpl<F> delta = lineB.start - lineA.start;

	Vec3_tpl<F> dirA = lineA.end - lineA.start;
	Vec3_tpl<F> dirB = lineB.end - lineB.start;

	F det = dirA.x * dirB.y - dirA.y * dirB.x;
	F detA = delta.x * dirB.y - delta.y * dirB.x;
	F detB = delta.x * dirA.y - delta.y * dirA.x;

	F absDet = fabs_tpl(det);

	if (absDet >= Epsilon)
	{
		F invDet = (F)1.0 / det;

		F a = detA * invDet;
		F b = detB * invDet;
		outA = a;
		outB = b;

		if ((a > (F)1.0) || (a < (F)0.0) || (b > (F)1.0) || (b < (F)0.0))
			return false;
	}
	else
	{
		outA = outB = (F)0.5;

		return false;
	}

	return true;
}

//! Calculates the intersection between a line segment and a polygon, in 2D (i.e. ignoring z coordinate).
//! The VecContainer should be a container of Vec3 such that we can traverse it using iterators.
//! \param[out] intersectionPoint is set to the intersection point or the end of the segment, if no intersection.
template<typename VecIterator>
inline bool Lineseg_Polygon2D(const Lineseg& lineseg, VecIterator polygonBegin, VecIterator polygonEnd, Vec3& intersectionPoint, Vec3* pNormal = NULL, bool bForceNormalOutwards = false)
{
	intersectionPoint = lineseg.end;
	bool gotIntersection = false;

	float tmin = 1.0f;

	VecIterator iend = polygonEnd;
	VecIterator li, linext;
	Lineseg intersectSegment;
	for (li = polygonBegin; li != iend; ++li)
	{
		linext = li;
		++linext;
		if (linext == iend)
			linext = polygonBegin;
		Lineseg segmentPoly(*li, *linext);
		float s, t;
		if (Intersect::Lineseg_Lineseg2D(lineseg, segmentPoly, s, t))
		{
			if (s < 0.00001f || s > 0.99999f || t < 0.00001f || t > 0.99999f)
				continue;
			if (s < tmin)
			{
				tmin = s;
				gotIntersection = true;
				intersectSegment = segmentPoly;
			}
		}
	}

	intersectionPoint = lineseg.start + tmin * (lineseg.end - lineseg.start);

	if (pNormal && gotIntersection)
	{
		Vec3 vPolyseg = intersectSegment.end - intersectSegment.start;
		Vec3 vIntSeg = (lineseg.end - lineseg.start);
		pNormal->x = vPolyseg.y;
		pNormal->y = -vPolyseg.x;
		pNormal->z = 0;
		pNormal->NormalizeSafe();
		// returns the normal towards the start point of the intersecting segment (if it's not forced to be outwards)
		if (!bForceNormalOutwards && vIntSeg.Dot(*pNormal) > 0)
		{
			pNormal->x = -pNormal->x;
			pNormal->y = -pNormal->y;
		}
	}
	return gotIntersection;
}

template<typename VecContainer>
inline bool Lineseg_Polygon2D(const Lineseg& lineseg, const VecContainer& polygon, Vec3& intersectionPoint, Vec3* pNormal = NULL, bool bForceNormalOutwards = false)
{
	return Lineseg_Polygon2D(lineseg, polygon.begin(), polygon.end(), intersectionPoint, pNormal, bForceNormalOutwards);
}

//! Calculates intersection between a line and a triangle.
//! IMPORTANT: this is a single-sided intersection test. That means its not enough that the triangle
//! and line overlap, its also important that the triangle is "visible" when you are looking along
//! the line-direction.
//! If you need a double-sided test, you'll have to call this function twice with
//! reversed order of triangle vertices.
//! \retval true and store the 3d-intersection point in "output"if there is an intersection.
//! \retval false if no intersectino (the value in "output" is then undefined).
inline bool Line_Triangle(const Line& line, const Vec3& v0, const Vec3& v1, const Vec3& v2, Vec3& output)
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

	if (AllGood < 0.0f)
		return false;

	float dt = edgeA.Dot(q) / dot;

	Vec3 result = (dir * dt) + line.pointonline;
	output = result;

	return true;
}

//! Calculates intersection between a ray and a triangle.
//! IMPORTANT: this is a single-sided intersection test. That means its not sufficient
//! that the triangle and rayt overlap, its also important that the triangle
//! is "visible" when you from the origin along the ray-direction.
//! If you need a double-sided test, you'll have to call this function twice with
//! reversed order of triangle vertices.
//! \retval true and store the 3d-intersection point in "output"if there is an intersection.
//! \retval false if no intersectino (the value in "output" is then undefined).
inline bool Ray_Triangle(const Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2, Vec3& output)
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
	output = result;

	float AfterStart = (result - ray.origin).Dot(dir);

	return AfterStart >= 0.0f;
}

//!	Calculates intersection between a line-segment and a triangle.
//!	IMPORTANT: this is a single-sided intersection test. That means its not sufficient
//!	that the triangle and line-segment overlap, its also important that the triangle
//!	is "visible" when you are looking along the linesegment from "start" to "end".
//!	\note If you need a double-sided test, you'll have to call this function twice with reversed order of triangle vertices.
//! \retval true and store the 3D-intersection point in "output" if there is an intersection. If pT is non-zero "t-value" (from 0-1) is returned.
//! \retval false if no intersection (the value in "output" is then undefined). If pT is non-zero it is returned unmodified.
inline bool Lineseg_Triangle(const Lineseg& lineseg, const Vec3& v0, const Vec3& v1, const Vec3& v2, Vec3& output,
                             float* outT = 0)
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
	output = result;

	float AfterStart = (result - lineseg.start).Dot(dir);
	float BeforeEnd = -(result - lineseg.end).Dot(dir);
	float Within = (float)__fsel(AfterStart, BeforeEnd, AfterStart);

	if (outT)
		*outT = dt;

	return Within >= 0.0f;
}

//! Just ONE intersection point is calculated, and thats the entry point Lineseg and AABB are assumed to be in the same space.
//! \retval 0x00 no intersection (output undefined).
//! \retval 0x01 intersection (intersection point in output).
//! \retval 0x02 start of Lineseg is inside the AABB (ls.start is output) .
inline uint8 Ray_AABB(const Ray& ray, const AABB& aabb, Vec3& output1)
{

	uint8 cflags;
	float cosine;
	Vec3 cut;

	// check if "ray.origin" is inside of AABB
	cflags = (ray.origin.x >= aabb.min.x) << 0;
	cflags |= (ray.origin.x <= aabb.max.x) << 1;
	cflags |= (ray.origin.y >= aabb.min.y) << 2;
	cflags |= (ray.origin.y <= aabb.max.y) << 3;
	cflags |= (ray.origin.z >= aabb.min.z) << 4;
	cflags |= (ray.origin.z <= aabb.max.z) << 5;
	if (cflags == 0x3f) { output1 = ray.origin; return 0x02; }

	// check intersection with planes
	for (int i = 0; i < 3; i++)
	{
		if ((ray.direction[i] > 0) && (ray.origin[i] < aabb.min[i]))
		{
			cosine = (-ray.origin[i] + aabb.min[i]) / ray.direction[i];
			cut[i] = aabb.min[i];
			cut[incm3(i)] = ray.origin[incm3(i)] + (ray.direction[incm3(i)] * cosine);
			cut[decm3(i)] = ray.origin[decm3(i)] + (ray.direction[decm3(i)] * cosine);
			if ((cut[incm3(i)] > aabb.min[incm3(i)]) && (cut[incm3(i)] < aabb.max[incm3(i)]) && (cut[decm3(i)] > aabb.min[decm3(i)]) && (cut[decm3(i)] < aabb.max[decm3(i)]))
			{
				output1 = cut;
				return 0x01;
			}
		}
		if ((ray.direction[i] < 0) && (ray.origin[i] > aabb.max[i]))
		{
			cosine = (+ray.origin[i] - aabb.max[i]) / ray.direction[i];
			cut[i] = aabb.max[i];
			cut[incm3(i)] = ray.origin[incm3(i)] - (ray.direction[incm3(i)] * cosine);
			cut[decm3(i)] = ray.origin[decm3(i)] - (ray.direction[decm3(i)] * cosine);
			if ((cut[incm3(i)] > aabb.min[incm3(i)]) && (cut[incm3(i)] < aabb.max[incm3(i)]) && (cut[decm3(i)] > aabb.min[decm3(i)]) && (cut[decm3(i)] < aabb.max[decm3(i)]))
			{
				output1 = cut;
				return 0x01;
			}
		}
	}
	return 0x00;  //no intersection
}

//! Just ONE intersection point is calculated, and thats the entry point Lineseg and OBB are assumed to be in the same space.
//! \retval 0x00 no intersection (output undefined).
//! \retval 0x01 intersection (intersection point in output).
//! \retval 0x02 start of Lineseg is inside the OBB (ls.start is output).
inline uint8 Ray_OBB(const Ray& ray, const Vec3& pos, const OBB& obb, Vec3& output1)
{

	AABB aabb(obb.c - obb.h, obb.c + obb.h);
	Ray aray((ray.origin - pos) * obb.m33, ray.direction * obb.m33);

	uint8 cflags;
	float cosine;
	Vec3 cut;

	// check if "aray.origin" is inside of AABB
	cflags = (aray.origin.x > aabb.min.x) << 0;
	cflags |= (aray.origin.x < aabb.max.x) << 1;
	cflags |= (aray.origin.y > aabb.min.y) << 2;
	cflags |= (aray.origin.y < aabb.max.y) << 3;
	cflags |= (aray.origin.z > aabb.min.z) << 4;
	cflags |= (aray.origin.z < aabb.max.z) << 5;
	if (cflags == 0x3f) { output1 = aray.origin;  return 0x02; }

	// check intersection with planes
	for (int i = 0; i < 3; i++)
	{
		if ((aray.direction[i] > 0) && (aray.origin[i] < aabb.min[i]))
		{
			cosine = (-aray.origin[i] + aabb.min[i]) / aray.direction[i];
			cut[i] = aabb.min[i];
			cut[incm3(i)] = aray.origin[incm3(i)] + (aray.direction[incm3(i)] * cosine);
			cut[decm3(i)] = aray.origin[decm3(i)] + (aray.direction[decm3(i)] * cosine);
			if ((cut[incm3(i)] > aabb.min[incm3(i)]) && (cut[incm3(i)] < aabb.max[incm3(i)]) && (cut[decm3(i)] > aabb.min[decm3(i)]) && (cut[decm3(i)] < aabb.max[decm3(i)]))
			{
				output1 = obb.m33 * cut + pos;
				return 0x01;
			}
		}
		if ((aray.direction[i] < 0) && (aray.origin[i] > aabb.max[i]))
		{
			cosine = (+aray.origin[i] - aabb.max[i]) / aray.direction[i];
			cut[i] = aabb.max[i];
			cut[incm3(i)] = aray.origin[incm3(i)] - (aray.direction[incm3(i)] * cosine);
			cut[decm3(i)] = aray.origin[decm3(i)] - (aray.direction[decm3(i)] * cosine);
			if ((cut[incm3(i)] > aabb.min[incm3(i)]) && (cut[incm3(i)] < aabb.max[incm3(i)]) && (cut[decm3(i)] > aabb.min[decm3(i)]) && (cut[decm3(i)] < aabb.max[decm3(i)]))
			{
				output1 = obb.m33 * cut + pos;
				return 0x01;
			}
		}
	}
	return 0x00;  //no intersection
}

//! Just ONE intersection point is calculated, and thats the entry point.
//! Lineseg and AABB are assumed to be in the same space.
//! \retval 0x00 = no intersection (output undefined).
//! \retval 0x01 = intersection (intersection point in output).
//! \retval 0x02 = start of Lineseg is inside the AABB (ls.start is output).
inline uint8 Lineseg_AABB(const Lineseg& ls, const AABB& aabb, Vec3& output1)
{

	uint8 cflags;
	float cosine;
	Vec3 cut;
	Vec3 lnormal = (ls.start - ls.end).GetNormalized();
	//--------------------------------------------------------------------------------------
	//----         check if "ls.start" is inside of AABB    ---------------------------
	//--------------------------------------------------------------------------------------
	cflags = (ls.start.x > aabb.min.x) << 0;
	cflags |= (ls.start.x < aabb.max.x) << 1;
	cflags |= (ls.start.y > aabb.min.y) << 2;
	cflags |= (ls.start.y < aabb.max.y) << 3;
	cflags |= (ls.start.z > aabb.min.z) << 4;
	cflags |= (ls.start.z < aabb.max.z) << 5;
	if (cflags == 0x3f)
	{
		//ls.start is inside of aabb
		output1 = ls.start;
		return 0x02;
	}

	//--------------------------------------------------------------------------------------
	//----         check intersection with x-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.x)
	{
		if ((ls.start.x < aabb.min.x) && (ls.end.x > aabb.min.x))
		{
			cosine = (-ls.start.x + (+aabb.min.x)) / lnormal.x;
			cut(aabb.min.x, ls.start.y + (lnormal.y * cosine), ls.start.z + (lnormal.z * cosine));
			//check if cut-point is inside YZ-plane border
			if ((cut.y > aabb.min.y) && (cut.y < aabb.max.y) && (cut.z > aabb.min.z) && (cut.z < aabb.max.z))
			{
				output1 = cut;
				return 0x01;
			}
		}
		if ((ls.start.x > aabb.max.x) && (ls.end.x < aabb.max.x))
		{
			cosine = (+ls.start.x + (-aabb.max.x)) / lnormal.x;
			cut(aabb.max.x, ls.start.y - (lnormal.y * cosine), ls.start.z - (lnormal.z * cosine));
			//check if cut-point is inside YZ-plane border
			if ((cut.y > aabb.min.y) && (cut.y < aabb.max.y) && (cut.z > aabb.min.z) && (cut.z < aabb.max.z))
			{
				output1 = cut;
				return 0x01;
			}
		}
	}
	//--------------------------------------------------------------------------------------
	//----         check intersection with z-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.z)
	{
		if ((ls.start.z < aabb.min.z) && (ls.end.z > aabb.min.z))
		{
			cosine = (-ls.start.z + (+aabb.min.z)) / lnormal.z;
			cut(ls.start.x + (lnormal.x * cosine), ls.start.y + (lnormal.y * cosine), aabb.min.z);
			//check if cut-point is inside XY-plane border
			if ((cut.x > aabb.min.x) && (cut.x < aabb.max.x) && (cut.y > aabb.min.y) && (cut.y < aabb.max.y))
			{
				output1 = cut;
				return 0x01;
			}
		}
		if ((ls.start.z > aabb.max.z) && (ls.end.z < aabb.max.z))
		{
			cosine = (+ls.start.z + (-aabb.max.z)) / lnormal.z;
			cut(ls.start.x - (lnormal.x * cosine), ls.start.y - (lnormal.y * cosine), aabb.max.z);
			//check if cut-point is inside XY-plane border
			if ((cut.x > aabb.min.x) && (cut.x < aabb.max.x) && (cut.y > aabb.min.y) && (cut.y < aabb.max.y))
			{
				output1 = cut;
				return 0x01;
			}
		}
	}
	//--------------------------------------------------------------------------------------
	//----         check intersection with y-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.y)
	{
		if ((ls.start.y < aabb.min.y) && (ls.end.y > aabb.min.y))
		{
			cosine = (-ls.start.y + (+aabb.min.y)) / lnormal.y;
			cut(ls.start.x + (lnormal.x * cosine), aabb.min.y, ls.start.z + (lnormal.z * cosine));
			//check if cut-point is inside XZ-plane border
			if ((cut.x > aabb.min.x) && (cut.x < aabb.max.x) && (cut.z > aabb.min.z) && (cut.z < aabb.max.z))
			{
				output1 = cut;
				return 0x01;
			}
		}
		if ((ls.start.y > aabb.max.y) && (ls.end.y < aabb.max.y))
		{
			cosine = (+ls.start.y + (-aabb.max.y)) / lnormal.y;
			cut(ls.start.x - (lnormal.x * cosine), aabb.max.y, ls.start.z - (lnormal.z * cosine));
			//check if cut-point is inside XZ-plane border
			if ((cut.x > aabb.min.x) && (cut.x < aabb.max.x) && (cut.z > aabb.min.z) && (cut.z < aabb.max.z))
			{
				output1 = cut;
				return 0x01;
			}
		}
	}
	//no intersection
	return 0x00;
}

//! Just ONE intersection point is calculated, and thats the entry point Lineseg and OBB are assumed to be in the same space.
//! \retval 0x00 no intersection (output undefined).
//! \retval 0x01 intersection (intersection point in output).
//! \retval 0x02 start of Lineseg is inside the OBB (ls.start is output).
inline uint8 Lineseg_OBB(const Lineseg& lseg, const Vec3& pos, const OBB& obb, Vec3& output1)
{

	AABB aabb(obb.c - obb.h, obb.c + obb.h);
	Lineseg ls((lseg.start - pos) * obb.m33, (lseg.end - pos) * obb.m33);

	uint8 cflags;
	float cosine;
	Vec3 cut;
	Vec3 lnormal = (ls.start - ls.end).GetNormalized();
	//--------------------------------------------------------------------------------------
	//----         check if "ls.start" is inside of AABB    ---------------------------
	//--------------------------------------------------------------------------------------
	cflags = (ls.start.x > aabb.min.x) << 0;
	cflags |= (ls.start.x < aabb.max.x) << 1;
	cflags |= (ls.start.y > aabb.min.y) << 2;
	cflags |= (ls.start.y < aabb.max.y) << 3;
	cflags |= (ls.start.z > aabb.min.z) << 4;
	cflags |= (ls.start.z < aabb.max.z) << 5;
	if (cflags == 0x3f)
	{
		//ls.start is inside of aabb
		output1 = obb.m33 * ls.start + pos;
		return 0x02;
	}

	//--------------------------------------------------------------------------------------
	//----         check intersection with x-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.x)
	{
		if ((ls.start.x < aabb.min.x) && (ls.end.x > aabb.min.x))
		{
			cosine = (-ls.start.x + (+aabb.min.x)) / lnormal.x;
			cut(aabb.min.x, ls.start.y + (lnormal.y * cosine), ls.start.z + (lnormal.z * cosine));
			//check if cut-point is inside YZ-plane border
			if ((cut.y > aabb.min.y) && (cut.y < aabb.max.y) && (cut.z > aabb.min.z) && (cut.z < aabb.max.z))
			{
				output1 = obb.m33 * cut + pos;
				return 0x01;
			}
		}
		if ((ls.start.x > aabb.max.x) && (ls.end.x < aabb.max.x))
		{
			cosine = (+ls.start.x + (-aabb.max.x)) / lnormal.x;
			cut(aabb.max.x, ls.start.y - (lnormal.y * cosine), ls.start.z - (lnormal.z * cosine));
			//check if cut-point is inside YZ-plane border
			if ((cut.y > aabb.min.y) && (cut.y < aabb.max.y) && (cut.z > aabb.min.z) && (cut.z < aabb.max.z))
			{
				output1 = obb.m33 * cut + pos;
				return 0x01;
			}
		}
	}
	//--------------------------------------------------------------------------------------
	//----         check intersection with z-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.z)
	{
		if ((ls.start.z < aabb.min.z) && (ls.end.z > aabb.min.z))
		{
			cosine = (-ls.start.z + (+aabb.min.z)) / lnormal.z;
			cut(ls.start.x + (lnormal.x * cosine), ls.start.y + (lnormal.y * cosine), aabb.min.z);
			//check if cut-point is inside XY-plane border
			if ((cut.x > aabb.min.x) && (cut.x < aabb.max.x) && (cut.y > aabb.min.y) && (cut.y < aabb.max.y))
			{
				output1 = obb.m33 * cut + pos;
				return 0x01;
			}
		}
		if ((ls.start.z > aabb.max.z) && (ls.end.z < aabb.max.z))
		{
			cosine = (+ls.start.z + (-aabb.max.z)) / lnormal.z;
			cut(ls.start.x - (lnormal.x * cosine), ls.start.y - (lnormal.y * cosine), aabb.max.z);
			//check if cut-point is inside XY-plane border
			if ((cut.x > aabb.min.x) && (cut.x < aabb.max.x) && (cut.y > aabb.min.y) && (cut.y < aabb.max.y))
			{
				output1 = obb.m33 * cut + pos;
				return 0x01;
			}
		}
	}
	//--------------------------------------------------------------------------------------
	//----         check intersection with y-planes           ------------------------------
	//--------------------------------------------------------------------------------------
	if (lnormal.y)
	{
		if ((ls.start.y < aabb.min.y) && (ls.end.y > aabb.min.y))
		{
			cosine = (-ls.start.y + (+aabb.min.y)) / lnormal.y;
			cut(ls.start.x + (lnormal.x * cosine), aabb.min.y, ls.start.z + (lnormal.z * cosine));
			//check if cut-point is inside XZ-plane border
			if ((cut.x > aabb.min.x) && (cut.x < aabb.max.x) && (cut.z > aabb.min.z) && (cut.z < aabb.max.z))
			{
				output1 = obb.m33 * cut + pos;
				return 0x01;
			}
		}
		if ((ls.start.y > aabb.max.y) && (ls.end.y < aabb.max.y))
		{
			cosine = (+ls.start.y + (-aabb.max.y)) / lnormal.y;
			cut(ls.start.x - (lnormal.x * cosine), aabb.max.y, ls.start.z - (lnormal.z * cosine));
			//check if cut-point is inside XZ-plane border
			if ((cut.x > aabb.min.x) && (cut.x < aabb.max.x) && (cut.z > aabb.min.z) && (cut.z < aabb.max.z))
			{
				output1 = obb.m33 * cut + pos;
				return 0x01;
			}
		}
	}
	//no intersection
	return 0x00;
}

//! \retval 0x00 no intersection.
//! \retval 0x01 not possible.
//! \retval 0x02 not possible.
//! \retval 0x03 two intersection, lineseg has ENTRY and EXIT point.
inline unsigned char Line_Sphere(const Line& line, const Sphere& s, Vec3& i0, Vec3& i1)
{

	Vec3 end = line.pointonline + line.direction;

	float a = line.direction | line.direction;
	float b = (line.direction | (line.pointonline - s.center)) * 2.0f;
	float c = ((line.pointonline - s.center) | (line.pointonline - s.center)) - (s.radius * s.radius);

	float desc = (b * b) - (4 * a * c);

	unsigned char intersection = 0;
	if (desc >= 0.0f)
	{
		float lamba0 = (-b - sqrt_tpl(desc)) / (2.0f * a);
		//_stprintf(d3dApp.token,"lamba0: %20.12f",lamba0);
		//d3dApp.m_pFont->DrawText( 2, d3dApp.PrintY, D3DCOLOR_ARGB(255,255,255,0), d3dApp.token );	d3dApp.PrintY+=20;
		i0 = line.pointonline + ((end - line.pointonline) * lamba0);
		intersection = 1;

		float lamba1 = (-b + sqrt_tpl(desc)) / (2.0f * a);
		//_stprintf(d3dApp.token,"lamba1: %20.12f",lamba1);
		//d3dApp.m_pFont->DrawText( 2, d3dApp.PrintY, D3DCOLOR_ARGB(255,255,255,0), d3dApp.token );	d3dApp.PrintY+=20;
		i1 = line.pointonline + ((end - line.pointonline) * lamba1);
		intersection |= 2;
	}

	return intersection;
}

//! \retval 0x00 = no intersection.
//! \retval 0x01 not possible.
//! \retval 0x02 one intersection, lineseg has just an EXIT point but no ENTRY point (ls.start is inside the sphere).
//! \retval 0x03 two intersection, lineseg has ENTRY and EXIT point.
inline unsigned char Ray_Sphere(const Ray& ray, const Sphere& s, Vec3& i0, Vec3& i1)
{
	Vec3 end = ray.origin + ray.direction;
	float a = ray.direction | ray.direction;
	float b = (ray.direction | (ray.origin - s.center)) * 2.0f;
	float c = ((ray.origin - s.center) | (ray.origin - s.center)) - (s.radius * s.radius);

	float desc = (b * b) - (4 * a * c);

	unsigned char intersection = 0;
	if (desc >= 0.0f)
	{
		float lamba0 = (-b - sqrt_tpl(desc)) / (2.0f * a);
		//	_stprintf(d3dApp.token,"lamba0: %20.12f",lamba0);
		//	d3dApp.m_pFont->DrawText( 2, d3dApp.PrintY, D3DCOLOR_ARGB(255,255,255,0), d3dApp.token );	d3dApp.PrintY+=20;
		if (lamba0 > 0.0f)
		{
			i0 = ray.origin + ((end - ray.origin) * lamba0);
			intersection = 1;
		}

		float lamba1 = (-b + sqrt_tpl(desc)) / (2.0f * a);
		//	_stprintf(d3dApp.token,"lamba1: %20.12f",lamba1);
		//	d3dApp.m_pFont->DrawText( 2, d3dApp.PrintY, D3DCOLOR_ARGB(255,255,255,0), d3dApp.token );	d3dApp.PrintY+=20;
		if (lamba1 > 0.0f)
		{
			i1 = ray.origin + ((end - ray.origin) * lamba1);
			intersection |= 2;
		}
	}
	return intersection;
}

inline bool Ray_SphereFirst(const Ray& ray, const Sphere& s, Vec3& intPoint)
{
	Vec3 p2;
	unsigned char res = Ray_Sphere(ray, s, intPoint, p2);
	if (res == 2) { intPoint = p2; }
	if (res > 1)  return true;
	return false;
}

//! \retval 0x00 no intersection.
//! \retval 0x01 one intersection, lineseg has just an ENTRY point but no EXIT point (ls.end is inside the sphere).
//! \retval 0x02 one intersection, lineseg has just an EXIT point but no ENTRY point (ls.start is inside the sphere).
//! \retval 0x03 two intersection, lineseg has ENTRY and EXIT point.
inline unsigned char Lineseg_Sphere(const Lineseg& ls, const Sphere& s, Vec3& i0, Vec3& i1)
{

	Vec3 dir = (ls.end - ls.start);

	float a = dir | dir;
	if (a == 0.0f)
		return 0;

	float b = (dir | (ls.start - s.center)) * 2.0f;
	float c = ((ls.start - s.center) | (ls.start - s.center)) - (s.radius * s.radius);
	float desc = (b * b) - (4 * a * c);

	unsigned char intersection = 0;
	if (desc >= 0.0f)
	{
		float lamba0 = (-b - sqrt_tpl(desc)) / (2.0f * a);
		if (lamba0 > 0.0f)
		{
			i0 = ls.start + ((ls.end - ls.start) * lamba0);
			//skip, if 1st cutting-point is "in front" of ls.end
			if (((i0 - ls.end) | dir) > 0) return 0;
			intersection = 0x01;
		}

		float lamba1 = (-b + sqrt_tpl(desc)) / (2.0f * a);
		if (lamba1 > 0.0f)
		{
			i1 = ls.start + ((ls.end - ls.start) * lamba1);
			//skip, if 2nd cutting-point is "in front" of ls.end (=ls.end is inside sphere)
			if (((i1 - ls.end) | dir) > 0) return intersection;
			intersection |= 0x02;
		}
	}
	return intersection;
}

inline bool Lineseg_SphereFirst(const Lineseg& lineseg, const Sphere& s, Vec3& intPoint)
{
	Vec3 p2;
	uint8 res = Lineseg_Sphere(lineseg, s, intPoint, p2);
	if (res == 2) { intPoint = p2; }
	if (res > 1)  return true;
	return false;
}

inline bool CheckConeRoot(float t, const Ray& ray, const Cone& c, float height)
{
	// hit is behind the ray, we are not interested in this
	if (t < 0.0f)
	{
		return false;
	}

	Vec3 isecp = ray.origin + t * ray.direction;
	float projvalue = (isecp - c.mTip) | c.mDir;

	return (projvalue >= 0 && projvalue <= height);
}

//! Ray - Cone intersection. Note: cone direction vector should be normalized
inline bool Ray_Cone(const Ray& ray, const Cone& c, Vec3& intPoint)
{
	const float epsilon = 0.00001f;

	// calculate square cosine from height/base radius of cone
	float radsq = c.mBaseRadius * c.mBaseRadius;
	float heightsq = c.mHeight * c.mHeight;
	float cossq = heightsq / (radsq + heightsq);

	// first check, see if origin is inside the cone
	Vec3 delta = ray.origin - c.mTip;
	float deltaLenSq = delta.len2();
	float heightProj = delta | c.mDir;

	if ((heightProj * heightProj / deltaLenSq > cossq) && (heightProj < c.mHeight) && (heightProj > 0))
	{
		return true;
	}

	// Ray origin is outside cone, check for intersection with sides
	Matrix33 m;

	// fill the matrix with outer product of cone direction vector - cos(theta)^2 * I
	float aa = c.mDir.x * c.mDir.x - cossq;
	float bb = c.mDir.y * c.mDir.y - cossq;
	float cc = c.mDir.z * c.mDir.z - cossq;
	float ab = c.mDir.x * c.mDir.y;
	float ac = c.mDir.x * c.mDir.z;
	float bc = c.mDir.y * c.mDir.z;
	m(aa, ab, ac, ab, bb, bc, ac, bc, cc);

	float c2 = ray.direction | (m * ray.direction);
	Vec3 tvec = m * delta;
	float c1 = ray.direction | tvec;
	float c0 = delta | tvec;

	float discriminant = c1 * c1 - c0 * c2;

	// no solution to quadratic polynomial
	if (discriminant < 0)
		return false;

	// special case where ray is parallel to cone surface
	if (fabs(c2) < epsilon)
	{
		// complete overlap between ray and cone surface (infinite solutions, but what we are interested in,
		// is that we have an intersection :) )
		if (fabs(c1) < epsilon)
		{
			return true;
		}

		// we have one intersection, keep if it's on the correct semicone
		float t = -c0 / c1;

		return CheckConeRoot(t, ray, c, c.mHeight);
	}
	else
	{
		// usual case, we have two roots, check if it's on correct semicone
		float discrRoot = sqrt(discriminant);
		float invc2 = 1.0f / c2;

		float t1 = (-c1 + discrRoot) * invc2;
		float t2 = (-c1 - discrRoot) * invc2;

		return CheckConeRoot(t1, ray, c, c.mHeight) || CheckConeRoot(t2, ray, c, c.mHeight);
	}
}
};

#endif //vector
