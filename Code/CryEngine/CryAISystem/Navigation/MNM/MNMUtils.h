// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/NavigationSystem/MNMTile.h>

namespace MNM
{
namespace Utils
{

template<typename Ty>
inline bool maximize(Ty& val, const Ty& x)
{
	if (val < x)
	{
		val = x;
		return true;
	}
	return false;
}

template<typename Ty>
inline bool minimize(Ty& val, const Ty& x)
{
	if (val > x)
	{
		val = x;
		return true;
	}
	return false;
}

template<typename Ty>
inline void sort2(Ty& x, Ty& y)
{
	if (x > y)
		std::swap(x, y);
}

template<typename Ty>
inline void rsort2(Ty& x, Ty& y)
{
	if (x < y)
		std::swap(x, y);
}

template<typename Ty>
inline Ty clamp(const Ty& x, const Ty& min, const Ty& max)
{
	if (x > max)
		return max;
	if (x < min)
		return min;
	return x;
}

template<typename Ty>
inline Ty clampunit(const Ty& x)
{
	if (x > Ty(1))
		return Ty(1);
	if (x < Ty(0))
		return Ty(0);
	return x;
}

template<typename Ty>
inline Ty next_mod3(const Ty& x)
{
	return (x + 1) % 3;
}

template<typename VecType>
inline real_t ProjectionPointLineSeg(const VecType& p, const VecType& a0, const VecType& a1)
{
	const VecType a = a1 - a0;
	const real_t lenSq = a.lenSq();

	if (lenSq > 0)
	{
		const VecType ap = p - a0;
		const real_t dot = a.dot(ap);
		return dot / lenSq;
	}
	return 0;
}

template<typename VecType>
inline real_t DistPointLineSegSq(const VecType& p, const VecType& a0, const VecType& a1)
{
	const VecType a = a1 - a0;
	const VecType ap = p - a0;
	const VecType bp = p - a1;

	const real_t e = ap.dot(a);
	if (e <= 0)
		return ap.dot(ap);

	const real_t f = a.dot(a);
	if (e >= f)
		return bp.dot(bp);

	return ap.dot(ap) - ((e * e) / f);
}

template<typename VecType>
inline VecType ClosestPtPointTriangle(const VecType& p, const VecType& a, const VecType& b, const VecType& c)
{
	const VecType ab = b - a;
	const VecType ac = c - a;
	const VecType ap = p - a;

	const real_t d1 = ab.dot(ap);
	const real_t d2 = ac.dot(ap);

	if ((d1 <= 0) && (d2 <= 0))
		return a;

	const VecType bp = p - b;
	const real_t d3 = ab.dot(bp);
	const real_t d4 = ac.dot(bp);

	if ((d3 >= 0) && (d4 <= d3))
		return b;

	const real_t vc = d1 * d4 - d3 * d2;
	if ((vc <= 0) && (d1 >= 0) && (d3 < 0))
	{
		const real_t v = d1 / (d1 - d3);
		return a + (ab * v);
	}

	const VecType cp = p - c;
	const real_t d5 = ab.dot(cp);
	const real_t d6 = ac.dot(cp);

	if ((d6 >= 0) && (d5 <= d6))
		return c;

	const real_t vb = d5 * d2 - d1 * d6;
	if ((vb <= 0) && (d2 >= 0) && (d6 < 0))
	{
		const real_t w = d2 / (d2 - d6);
		return a + (ac * w);
	}

	const real_t va = d3 * d6 - d5 * d4;
	if ((va <= 0) && ((d4 - d3) >= 0) && ((d5 - d6) > 0))
	{
		const real_t w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return b + ((c - b) * w);
	}

	// do not factorize the divisions : Fixed point precision requires it
	const real_t denom = va + vb + vc;
	const real_t v = vb / denom;
	const real_t w = vc / denom;

	return a + ab * v + ac * w;
}

inline bool PointInTriangle(const vector2_t& p, const vector2_t& a, const vector2_t& b, const vector2_t& c)
{
	// Careful: this function can return that the point p is inside triangle abc also in the case of degenerate triangle.
	const real_t e0 = (p - a).cross(a - b);
	const real_t e1 = (p - b).cross(b - c);
	const real_t e2 = (p - c).cross(c - a);

	// Using bitwise operators instead of logical for performance reasons.
	const bool hasNegative = e0 < 0 | e1 < 0 | e2 < 0;
	const bool hasPositive = e0 > 0 | e1 > 0 | e2 > 0;

	return !(hasNegative & hasPositive);
}

//! Projects point p on triangle abc in vertical direction. 
//! Returns true when the position is successfully projected to triangle or false in case of degenerate triangle.
//! The function isn't checking if the position p lies inside the triangle, it is responsibility of the caller to provide correct inputs.
inline bool ProjectPointOnTriangleVertical(const vector3_t& p, const vector3_t& a, const vector3_t& b, const vector3_t& c, vector3_t& projected)
{
	projected.x = p.x;
	projected.y = p.y;

	if (a.z == b.z && a.z == c.z)
	{
		projected.z = a.z;
		return true;
	}

	const vector3_t v0 = c - a;
	const vector3_t v1 = b - a;
	const vector3_t v2 = p - a;

	// Barycentric coordinates of the point p are computed in 2D space first
	const vector2_t v0_2d(v0);
	const vector2_t v1_2d(v1);
	const vector2_t v2_2d(v2);

	const real_t dot00 = v0_2d.dot(v0_2d);
	const real_t dot01 = v0_2d.dot(v1_2d);
	const real_t dot11 = v1_2d.dot(v1_2d);

	const real_t denom = dot00 * dot11 - dot01 * dot01;
	const real_t tolerance = real_t::epsilon() * 4;
	if (denom > -tolerance && denom < tolerance)
	{
		// Points are collinear, triangle is degenerate
		return false;
	}

	const real_t dot02 = v0_2d.dot(v2_2d);
	const real_t dot12 = v1_2d.dot(v2_2d);

	const real_t u = (dot11 * dot02 - dot01 * dot12);
	const real_t v = (dot00 * dot12 - dot01 * dot02);

	CRY_ASSERT_MESSAGE(PointInTriangle(vector2_t(p), vector2_t(a), vector2_t(b), vector2_t(c)), "Projecting point isn't lying inside the triangle.");

	// Compute only z value of the projected position, x and y stays the same
	projected.z = a.z + v0.z * u / denom + v1.z * v / denom;
	return true;
}

template<typename VecType>
inline real_t ClosestPtSegmentSegment(const VecType& a0, const VecType& a1, const VecType& b0, const VecType& b1,
	real_t& s, real_t& t, VecType& closesta, VecType& closestb)
{
	const VecType da = a1 - a0;
	const VecType db = b1 - b0;
	const VecType r = a0 - b0;
	const real_t a = da.lenSq();
	const real_t e = db.lenSq();
	const real_t f = db.dot(r);

	if ((a == 0) && (e == 0))
	{
		s = t = 0;
		closesta = a0;
		closestb = b0;

		return (closesta - closestb).lenSq();
	}

	if (a == 0)
	{
		s = 0;
		t = f / e;
		t = clampunit(t);
	}
	else
	{
		const real_t c = da.dot(r);

		if (e == 0)
		{
			t = 0;
			s = clampunit(-c / a);
		}
		else
		{
			const real_t b = da.dot(db);
			const real_t denom = (a * e) - (b * b);

			if (denom != 0)
				s = clampunit(((b * f) - (c * e)) / denom);
			else
				s = 0;

			const real_t tnom = (b * s) + f;

			if (tnom < 0)
			{
				t = 0;
				s = clampunit(-c / a);
			}
			else if (tnom > e)
			{
				t = 1;
				s = clampunit((b - c) / a);
			}
			else
				t = tnom / e;
		}
	}

	closesta = a0 + da * s;
	closestb = b0 + db * t;

	return (closesta - closestb).lenSq();
}

template<typename VecType>
inline vector2_t ClosestPtPointSegment(const VecType& p, const VecType& a0, const VecType& a1, real_t& t)
{
	const VecType a = a1 - a0;

	t = (p - a0).dot(a);

	if (t <= 0)
	{
		t = 0;
		return a0;
	}
	else
	{
		const real_t denom = a.lenSq();

		if (t >= denom)
		{
			t = 1;
			return a1;
		}
		else
		{
			t = t / denom;
			return a0 + (a * t);
		}
	}
}

// Intersection between two segments in 2D. The two parametric values are set to between 0 and 1
// if intersection occurs. If intersection does not occur their values will indicate the
// parametric values for intersection of the lines extended beyond the segment lengths.
// Parallel lines will result in a negative result.
enum EIntersectionResult
{
	eIR_NoIntersection,
	eIR_Intersection,
	eIR_ParallelOrCollinearSegments,
};

template<typename VecType>
inline EIntersectionResult DetailedIntersectSegmentSegment(const VecType& a0, const VecType& a1, const VecType& b0, const VecType& b1,
	real_t& s, real_t& t)
{
	/*
			  a0
			  |
			  s
	   b0-----t----------b1
			  |
			  |
			  a1

	   Assuming
	   da = a1 - a0
	   db = b1 - b0
	   we can define the equations of the two segments as
	   a0 + s * da = 0    with s = [0...1]
	   b0 + t * db = 0    with t = [0...1]
	   We have an intersection if

	   a0 + s * da = b0 + t * db

	   We then cross product both side for db ( to calculate s) obtaining

	   (a0 - b0) x db + s * (da x db) = t * (db x db)
	   (a0 - b0) x db + s * (da x db) = 0

	   we call e = (a0 - b0)

	   s = ( e x db ) / ( da x db )

	   We can now calculate t with the same procedure.
	 */

	const VecType e = b0 - a0;
	const VecType da = a1 - a0;
	const VecType db = b1 - b0;
	// | da.x   da.y |
	// | db.x   db.y |
	// If det is zero then the two vectors are dependent, meaning
	// they are parallel or colinear.
	const real_t det = da.x * db.y - da.y * db.x;
	const real_t tolerance = real_t::epsilon();
	const real_t signAdjustment = det >= real_t(0.0f) ? real_t(1.0f) : real_t(-1.0f);
	const real_t minAllowedValue = real_t(.0f) - tolerance;
	const real_t maxAllowedValue = (real_t(1.0f) * det * signAdjustment) + tolerance;
	if (det != 0)
	{
		s = (e.x * db.y - e.y * db.x) * signAdjustment;

		if ((s < minAllowedValue) || (s > maxAllowedValue))
			return eIR_NoIntersection;

		t = (e.x * da.y - e.y * da.x) * signAdjustment;
		if ((t < minAllowedValue) || (t > maxAllowedValue))
			return eIR_NoIntersection;

		s = clampunit(s / det * signAdjustment);
		t = clampunit(t / det * signAdjustment);
		return eIR_Intersection;
	}
	return eIR_ParallelOrCollinearSegments;
}

template<typename VecType>
inline bool IntersectSegmentSegment(const VecType& a0, const VecType& a1, const VecType& b0, const VecType& b1,
	real_t& s, real_t& t)
{
	return DetailedIntersectSegmentSegment(a0, a1, b0, b1, s, t) == eIR_Intersection;
}

inline const MNM::real_t CalculateMinHorizontalRange(const uint16 radiusVoxelUnits, const float voxelHSize)
{
	assert(voxelHSize > 0.0f);
	// The horizontal x-y size for the voxels it's recommended to be the same.
	return MNM::real_t(max(radiusVoxelUnits * voxelHSize * 2.0f, 1.0f));
}

inline const MNM::real_t CalculateMinVerticalRange(const uint16 agentHeightVoxelUnits, const float voxelVSize)
{
	assert(voxelVSize > 0.0f);
	return MNM::real_t(max(agentHeightVoxelUnits * voxelVSize, 1.0f));
}

} // namespace Utils
} // namespace MNM

