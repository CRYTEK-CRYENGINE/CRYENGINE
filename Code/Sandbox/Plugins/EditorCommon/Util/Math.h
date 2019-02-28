// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//! Half PI
#define PI_HALF (3.1415926535897932384626433832795f / 2.0f)

//! Epsilon for vector comparasion.
#define FLOAT_EPSILON 0.000001f

#include <CryMath/Cry_Geo.h>

//!  Compare two vectors if they are equal.
inline bool IsVectorsEqual(const Vec3& v1, const Vec3& v2, const float aEpsilon = FLOAT_EPSILON)
{
	return (fabs(v2.x - v1.x) < aEpsilon && fabs(v2.y - v1.y) < aEpsilon && fabs(v2.z - v1.z) < aEpsilon);
}


// Math utilities.

inline float PointToLineDistance2D(const Vec3& p1, const Vec3& p2, const Vec3& p3)
{
	float dx = p2.x - p1.x;
	float dy = p2.y - p1.y;
	if (dx + dy == 0)
		return (float)sqrt((p3.x - p1.x) * (p3.x - p1.x) + (p3.y - p1.y) * (p3.y - p1.y));
	float u = ((p3.x - p1.x) * dx + (p3.y - p1.y) * dy) / (dx * dx + dy * dy);
	if (u < 0)
		return (float)sqrt((p3.x - p1.x) * (p3.x - p1.x) + (p3.y - p1.y) * (p3.y - p1.y));
	else if (u > 1)
		return (float)sqrt((p3.x - p2.x) * (p3.x - p2.x) + (p3.y - p2.y) * (p3.y - p2.y));
	else
	{
		float x = p1.x + u * dx;
		float y = p1.y + u * dy;
		return (float)sqrt((p3.x - x) * (p3.x - x) + (p3.y - y) * (p3.y - y));
	}
}

inline float PointToLineDistance(const Vec3& p1, const Vec3& p2, const Vec3& p3)
{
	Vec3 d = p2 - p1;
	float u = d.Dot(p3 - p1) / (d).GetLengthSquared();
	if (u < 0)
		return (p3 - p1).GetLength();
	else if (u > 1)
		return (p3 - p2).GetLength();
	else
	{
		Vec3 p = p1 + u * d;
		return (p3 - p).GetLength();
	}
}

//! Calculate distance between point and line.
//! \param p1 Source line point.
//! \param p2 Target line point.
//! \param p3 Point to find intersection with.
//! \param intersectPoint Intersection point on the line.
//! \return Distance between point and line.
 inline float PointToLineDistance(const Vec3& p1, const Vec3& p2, const Vec3& p3, Vec3& intersectPoint)
{
	Vec3 d = p2 - p1;
	float fLength2 = d.GetLengthSquared();

	if (fLength2 < 0.00001f)
	{
		// p1-p2 is degenerated to a point
		intersectPoint = p1;
		return (p3 - p1).GetLength();
	}

	float u = d.Dot(p3 - p1) / fLength2;
	if (u < 0)
	{
		intersectPoint = p1;
		return (p3 - p1).GetLength();
	}
	else if (u > 1)
	{
		intersectPoint = p2;
		return (p3 - p2).GetLength();
	}
	else
	{
		Vec3 p = p1 + u * d;
		intersectPoint = p;
		return (p3 - p).GetLength();
	}
}

 //! Calculate the line segment PaPb that is the shortest route between
 //! two lines P1P2 and P3P4. Calculate also the values of mua and mub where
 //!    Pa = P1 + mua (P2 - P1)
 //!    Pb = P3 + mub (P4 - P3)
 //! 
 //! \param Source point of first line.
 //! \param Target point of first line.
 //! \param Source point of second line.
 //! \param Target point of second line.
 //! \return FALSE if no solution exists.
inline bool LineLineIntersect(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4,
                              Vec3& pa, Vec3& pb, float& mua, float& mub)
{
	Vec3 p13, p43, p21;
	float d1343, d4321, d1321, d4343, d2121;
	float numer, denom;

	p13.x = p1.x - p3.x;
	p13.y = p1.y - p3.y;
	p13.z = p1.z - p3.z;
	p43.x = p4.x - p3.x;
	p43.y = p4.y - p3.y;
	p43.z = p4.z - p3.z;
	if (fabs(p43.x) < LINE_EPS && fabs(p43.y) < LINE_EPS && fabs(p43.z) < LINE_EPS)
		return false;
	p21.x = p2.x - p1.x;
	p21.y = p2.y - p1.y;
	p21.z = p2.z - p1.z;
	if (fabs(p21.x) < LINE_EPS && fabs(p21.y) < LINE_EPS && fabs(p21.z) < LINE_EPS)
		return false;

	d1343 = p13.x * p43.x + p13.y * p43.y + p13.z * p43.z;
	d4321 = p43.x * p21.x + p43.y * p21.y + p43.z * p21.z;
	d1321 = p13.x * p21.x + p13.y * p21.y + p13.z * p21.z;
	d4343 = p43.x * p43.x + p43.y * p43.y + p43.z * p43.z;
	d2121 = p21.x * p21.x + p21.y * p21.y + p21.z * p21.z;

	denom = d2121 * d4343 - d4321 * d4321;
	if (fabs(denom) < LINE_EPS)
		return(FALSE);
	numer = d1343 * d4321 - d1321 * d4343;

	mua = numer / denom;
	mub = (d1343 + d4321 * (mua)) / d4343;

	pa.x = p1.x + mua * p21.x;
	pa.y = p1.y + mua * p21.y;
	pa.z = p1.z + mua * p21.z;
	pb.x = p3.x + mub * p43.x;
	pb.y = p3.y + mub * p43.y;
	pb.z = p3.z + mub * p43.z;

	return true;
}

//! Calculates shortest distance between ray and a arbitrary line segment.
//! \param raySrc Source point of ray.
//! \param rayTrg Target point of ray.
//! \param p1 First point of line segment.
//! \param p2 Second point of line segment.
//! \param intersectPoint This parameter returns nearest point on line segment to ray.
//! return distance fro ray to line segment.
inline float RayToLineDistance(const Vec3& raySrc, const Vec3& rayTrg, const Vec3& p1, const Vec3& p2, Vec3& nearestPoint)
{
	Vec3 intPnt;
	Vec3 rayLineP1 = raySrc;
	Vec3 rayLineP2 = rayTrg;
	Vec3 pa, pb;
	float ua, ub;

	if (!LineLineIntersect(p1, p2, rayLineP1, rayLineP2, pa, pb, ua, ub))
		return FLT_MAX;

	float d = 0;
	if (ua < 0)
		d = PointToLineDistance(rayLineP1, rayLineP2, p1, intPnt);
	else if (ua > 1)
		d = PointToLineDistance(rayLineP1, rayLineP2, p2, intPnt);
	else
	{
		intPnt = rayLineP1 + ub * (rayLineP2 - rayLineP1);
		d = (pb - pa).GetLength();
	}
	nearestPoint = intPnt;
	return d;
}

inline Matrix34 MatrixFromVector(const Vec3& dir, const Vec3& up = Vec3(0, 0, 1.0f), float rollAngle = 0)
{
	// LookAt transform.
	Vec3 xAxis, yAxis, zAxis;
	Vec3 upVector = up;

	if (dir.IsZero())
	{
		Matrix33 tm;
		tm.SetIdentity();
		return tm;
	}

	yAxis = dir.GetNormalized();

	if (yAxis.x == 0 && yAxis.y == 0) upVector.Set(-yAxis.z, 0, 0);

	xAxis = upVector.Cross(yAxis).GetNormalized();
	zAxis = xAxis.Cross(yAxis).GetNormalized();

	Matrix33 tm;
	tm.SetFromVectors(xAxis, yAxis, zAxis);

	if (rollAngle != 0)
	{
		Matrix33 RollMtx;
		RollMtx.SetRotationY(rollAngle);

		// Matrix multiply.
		tm = RollMtx * tm;
	}

	return tm;
}

//! Snaps rotation matrix to the nearest multiple of the given angle
//! \param orientation Source rotation matrix.
//! \param step angle in radians to snap to.
inline Matrix33 SnapToNearest(const Matrix33& orientation, float step)
{
	if (step <= std::numeric_limits<float>::min())
	{
		return orientation;
	}

	Ang3 euler = Ang3::GetAnglesXYZ(orientation);
	euler /= step;
	euler.x = std::round(euler.x);
	euler.y = std::round(euler.y);
	euler.z = std::round(euler.z);
	euler *= step;
	return Matrix33(euler);
}

//TODO: could we include this function in the core engine math Intersect namespace ?
namespace Intersect
{
//! handy function for Ray AABB intersection, the Ray object is created inside
inline uint8 Ray_AABB(const Vec3& rRayStart, const Vec3& rRayDir, const AABB& rBox, Vec3& rOutPt)
{
	return Ray_AABB(Ray(rRayStart, rRayDir), rBox, rOutPt);
}

//! Check if ray intersect edge of bounding box.
//! \param epsilonDist if distance between ray and edge is less then this epsilon then edge was intersected.
//! \param dist Distance between ray and edge.
//! \param intPnt intersection point.
inline bool Ray_AABBEdge(const Vec3& raySrc, const Vec3& rayDir, const AABB& aabb, float epsilonDist, float& dist, Vec3& intPnt)
{
	// Check 6 group lines.
	Vec3 rayTrg = raySrc + rayDir * 10000.0f;
	Vec3 pnt[12];

	float d[12];

	// Near
	d[0] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.min.x, aabb.min.y, aabb.max.z), Vec3(aabb.max.x, aabb.min.y, aabb.max.z), pnt[0]);
	d[1] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.min.x, aabb.max.y, aabb.max.z), Vec3(aabb.max.x, aabb.max.y, aabb.max.z), pnt[1]);
	d[2] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.min.x, aabb.min.y, aabb.max.z), Vec3(aabb.min.x, aabb.max.y, aabb.max.z), pnt[2]);
	d[3] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.max.x, aabb.min.y, aabb.max.z), Vec3(aabb.max.x, aabb.max.y, aabb.max.z), pnt[3]);

	// Far
	d[4] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.min.x, aabb.min.y, aabb.min.z), Vec3(aabb.max.x, aabb.min.y, aabb.min.z), pnt[4]);
	d[5] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.min.x, aabb.max.y, aabb.min.z), Vec3(aabb.max.x, aabb.max.y, aabb.min.z), pnt[5]);
	d[6] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.min.x, aabb.min.y, aabb.min.z), Vec3(aabb.min.x, aabb.max.y, aabb.min.z), pnt[6]);
	d[7] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.max.x, aabb.min.y, aabb.min.z), Vec3(aabb.max.x, aabb.max.y, aabb.min.z), pnt[7]);

	// Sides.
	d[8] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.min.x, aabb.min.y, aabb.min.z), Vec3(aabb.min.x, aabb.min.y, aabb.max.z), pnt[8]);
	d[9] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.max.x, aabb.min.y, aabb.min.z), Vec3(aabb.max.x, aabb.min.y, aabb.max.z), pnt[9]);
	d[10] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.min.x, aabb.max.y, aabb.min.z), Vec3(aabb.min.x, aabb.max.y, aabb.max.z), pnt[10]);
	d[11] = RayToLineDistance(raySrc, rayTrg, Vec3(aabb.max.x, aabb.max.y, aabb.min.z), Vec3(aabb.max.x, aabb.max.y, aabb.max.z), pnt[11]);

	dist = FLT_MAX;
	for (int i = 0; i < 12; i++)
	{
		if (d[i] < dist)
		{
			dist = d[i];
			intPnt = pnt[i];
		}
	}
	if (dist < epsilonDist)
	{
		return true;
	}
	return false;
}

};

inline int gcd(int a, int b)
{
	int c = a % b;
	while (c != 0)
	{
		a = b;
		b = c;
		c = a % b;
	}
	return b;
}

