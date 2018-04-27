// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GeometryUtil.h"

#if 0
struct SPointSorter
{
	SPointSorter(const Vec3& pt) : pt(pt) {}
	bool operator()(const Vec3& lhs, const Vec3& rhs)
	{
		float isLeft = IsLeft(pt, lhs, rhs);
		if (isLeft > 0.0f)
			return true;
		else if (isLeft < 0.0f)
			return false;
		else
			return (lhs - pt).GetLengthSquared2D() < (rhs - pt).GetLengthSquared2D();
	}
	const Vec3 pt;
};

//===================================================================
// ConvexHull2D
// Implements Graham's scan
//===================================================================
void ConvexHull2DGraham(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn)
{
	CRY_PROFILE_FUNCTION( PROFILE_AI);
	const unsigned nPtsIn = ptsIn.size();
	if (nPtsIn < 3)
	{
		ptsOut = ptsIn;
		return;
	}
	unsigned iBotRight = 0;
	for (unsigned iPt = 1; iPt < nPtsIn; ++iPt)
	{
		if (ptsIn[iPt].y < ptsIn[iBotRight].y)
			iBotRight = iPt;
		else if (ptsIn[iPt].y == ptsIn[iBotRight].y && ptsIn[iPt].x < ptsIn[iBotRight].x)
			iBotRight = iPt;
	}

	static std::vector<Vec3> ptsSorted; // avoid memory allocation
	ptsSorted.assign(ptsIn.begin(), ptsIn.end());

	std::swap(ptsSorted[0], ptsSorted[iBotRight]);
	{
		FRAME_PROFILER("SORT Graham", gEnv->pSystem, PROFILE_AI)
		std::sort(ptsSorted.begin() + 1, ptsSorted.end(), SPointSorter(ptsSorted[0]));
	}
	ptsSorted.erase(std::unique(ptsSorted.begin(), ptsSorted.end(), ptEqual), ptsSorted.end());

	const unsigned nPtsSorted = ptsSorted.size();
	if (nPtsSorted < 3)
	{
		ptsOut = ptsSorted;
		return;
	}

	ptsOut.resize(0);
	ptsOut.push_back(ptsSorted[0]);
	ptsOut.push_back(ptsSorted[1]);
	unsigned int i = 2;
	while (i < nPtsSorted)
	{
		if (ptsOut.size() <= 1)
		{
			AIWarning("Badness in ConvexHull2D");
			AILogComment("i = %d ptsIn = ", i);
			for (unsigned j = 0; j < ptsIn.size(); ++j)
				AILogComment("%6.3f, %6.3f, %6.3f", ptsIn[j].x, ptsIn[j].y, ptsIn[j].z);
			AILogComment("ptsSorted = ");
			for (unsigned j = 0; j < ptsSorted.size(); ++j)
				AILogComment("%6.3f, %6.3f, %6.3f", ptsSorted[j].x, ptsSorted[j].y, ptsSorted[j].z);
			ptsOut.resize(0);
			return;
		}
		const Vec3& pt1 = ptsOut[ptsOut.size() - 1];
		const Vec3& pt2 = ptsOut[ptsOut.size() - 2];
		const Vec3& p = ptsSorted[i];
		float isLeft = IsLeft(pt2, pt1, p);
		if (isLeft > 0.0f)
		{
			ptsOut.push_back(p);
			++i;
		}
		else if (isLeft < 0.0f)
		{
			ptsOut.pop_back();
		}
		else
		{
			ptsOut.pop_back();
			ptsOut.push_back(p);
			++i;
		}
	}
}
#endif

//===================================================================
// IsLeft: tests if a point is Left|On|Right of an infinite line.
//    Input:  three points P0, P1, and P2
//    Return: >0 for P2 left of the line through P0 and P1
//            =0 for P2 on the line
//            <0 for P2 right of the line
//===================================================================
inline float IsLeft(Vec3 P0, Vec3 P1, const Vec3& P2)
{
	bool swap = false;
	if (P0.x < P1.x)
		swap = true;
	else if (P0.x == P1.x && P0.y < P1.y)
		swap = true;

	if (swap)
		std::swap(P0, P1);

	float res = (P1.x - P0.x) * (P2.y - P0.y) - (P2.x - P0.x) * (P1.y - P0.y);
	const float tol = 0.0000f;
	if (res > tol || res < -tol)
		return swap ? -res : res;
	else
		return 0.0f;
}

inline bool ptEqual(const Vec3& lhs, const Vec3& rhs)
{
	const float tol = 0.01f;
	return (fabs(lhs.x - rhs.x) < tol) && (fabs(lhs.y - rhs.y) < tol);
}

inline float IsLeftAndrew(const Vec3& p0, const Vec3& p1, const Vec3& p2)
{
	return (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);
}

inline bool PointSorterAndrew(const Vec3& lhs, const Vec3& rhs)
{
	if (lhs.x < rhs.x) return true;
	if (lhs.x > rhs.x) return false;
	return lhs.y < rhs.y;
}

//===================================================================
// ConvexHull2D
// Implements Andrew's algorithm
//
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
//===================================================================
EDITOR_COMMON_API void ConvexHull2DAndrew(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	const int n = (int)ptsIn.size();
	if (n < 3)
	{
		ptsOut = ptsIn;
		return;
	}

	std::vector<Vec3> P = ptsIn;
	{
		CRY_PROFILE_SECTION(PROFILE_AI, "SORT Andrew")
		std::sort(P.begin(), P.end(), PointSorterAndrew);
	}

	// the output array ptsOut[] will be used as the stack
	int i;

	ptsOut.clear();
	ptsOut.reserve(P.size());

	// Get the indices of points with min x-coord and min|max y-coord
	int minmin = 0, minmax;
	float xmin = P[0].x;
	for (i = 1; i < n; i++)
		if (P[i].x != xmin)
			break;

	minmax = i - 1;
	if (minmax == n - 1)
	{
		// degenerate case: all x-coords == xmin
		ptsOut.push_back(P[minmin]);
		if (P[minmax].y != P[minmin].y) // a nontrivial segment
			ptsOut.push_back(P[minmax]);
		ptsOut.push_back(P[minmin]);           // add polygon endpoint
		return;
	}

	// Get the indices of points with max x-coord and min|max y-coord
	int maxmin, maxmax = n - 1;
	float xmax = P[n - 1].x;
	for (i = n - 2; i >= 0; i--)
		if (P[i].x != xmax) break;
	maxmin = i + 1;

	// Compute the lower hull on the stack H
	ptsOut.push_back(P[minmin]);      // push minmin point onto stack
	i = minmax;
	while (++i <= maxmin)
	{
		// the lower line joins P[minmin] with P[maxmin]
		if (IsLeftAndrew(P[minmin], P[maxmin], P[i]) >= 0 && i < maxmin)
			continue;          // ignore P[i] above or on the lower line

		while ((int)ptsOut.size() > 1) // there are at least 2 points on the stack
		{
			// test if P[i] is left of the line at the stack top
			if (IsLeftAndrew(ptsOut[ptsOut.size() - 2], ptsOut.back(), P[i]) > 0)
				break;         // P[i] is a new hull vertex
			else
				ptsOut.pop_back(); // pop top point off stack
		}
		ptsOut.push_back(P[i]);       // push P[i] onto stack
	}

	// Next, compute the upper hull on the stack H above the bottom hull
	if (maxmax != maxmin)             // if distinct xmax points
		ptsOut.push_back(P[maxmax]);    // push maxmax point onto stack
	int bot = (int)ptsOut.size() - 1; // the bottom point of the upper hull stack
	i = maxmin;
	while (--i >= minmax)
	{
		// the upper line joins P[maxmax] with P[minmax]
		if (IsLeftAndrew(P[maxmax], P[minmax], P[i]) >= 0 && i > minmax)
			continue;          // ignore P[i] below or on the upper line

		while ((int)ptsOut.size() > bot + 1)    // at least 2 points on the upper stack
		{
			// test if P[i] is left of the line at the stack top
			if (IsLeftAndrew(ptsOut[ptsOut.size() - 2], ptsOut.back(), P[i]) > 0)
				break;         // P[i] is a new hull vertex
			else
				ptsOut.pop_back();         // pop top po2int off stack
		}
		ptsOut.push_back(P[i]);       // push P[i] onto stack
	}
	if (minmax != minmin)
		ptsOut.push_back(P[minmin]);  // push joining endpoint onto stack
	if (!ptsOut.empty() && ptEqual(ptsOut.front(), ptsOut.back()))
		ptsOut.pop_back();
}

