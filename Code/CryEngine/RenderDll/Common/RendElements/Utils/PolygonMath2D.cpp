// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Includes
#include "StdAfx.h"
#include "PolygonMath2D.h"

#ifndef RELEASE
	#define ASSERT_NUM_POLY_SIDES(numPts) { if (numPts < 0 || numPts > POLY_ARRAY_SIZE) {                  \
	  CRY_ASSERT_MESSAGE(0, "[BreakGlassSystem Error]: Polygon too large, need to increase array sizes."); \
	  CryLogAlways("[BreakGlassSystem Error]: Polygon too large, need to increase array sizes."); }        \
}
#else
	#define ASSERT_NUM_POLY_SIDES(numPts)
#endif

//--------------------------------------------------------------------------------------------------
// Name: PointInTriangle2D
// Desc: Determines if a point is in the specified triangle
//--------------------------------------------------------------------------------------------------
bool PointInTriangle2D(const Vec2& pt, const Vec2& a, const Vec2& b, const Vec2& c)
{
	float ax, ay, bx, by, cx, cy, apx, apy, bpx, bpy, cpx, cpy;
	float cCROSSap, bCROSScp, aCROSSbp;

	ax = c.x - b.x;
	ay = c.y - b.y;
	bx = a.x - c.x;
	by = a.y - c.y;
	cx = b.x - a.x;
	cy = b.y - a.y;

	apx = pt.x - a.x;
	apy = pt.y - a.y;
	bpx = pt.x - b.x;
	bpy = pt.y - b.y;
	cpx = pt.x - c.x;
	cpy = pt.y - c.y;

	aCROSSbp = ax * bpy - ay * bpx;
	cCROSSap = cx * apy - cy * apx;
	bCROSScp = bx * cpy - by * cpx;

	float ra = (float)__fsel(aCROSSbp, 1.0f, 0.0f);
	float rb = (float)__fsel(cCROSSap, 1.0f, 0.0f);
	float rc = (float)__fsel(bCROSScp, 1.0f, 0.0f);

	return (ra + rb + rc) > 2.5f;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: TriangleIsConvex2D
// Desc: Determines if a triangle is convex (angle at B less than 180 degrees)
//--------------------------------------------------------------------------------------------------
bool TriangleIsConvex2D(const Vec2& a, const Vec2& b, const Vec2& c)
{
	const Vec2 ba = a - b;
	const Vec2 bc = c - b;

	const float cross = ba.x * bc.y - bc.x * ba.y;
	return (cross < 0.0f);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: LineCircleIntersect2D
// Desc: Finds the intersection points (if any) between a line and circle
// Note: Returns the number of intersection points (0, 1 or 2)
//--------------------------------------------------------------------------------------------------
int LineCircleIntersect2D(const Vec2& pt0, const Vec2& pt1, const Vec2& center, const float radiusSq, float& intersectA, float& intersectB)
{
	int numIntersects = 0;

	const Vec2 lineSegment = pt1 - pt0;
	const Vec2 circleToLine = pt0 - center;

	// Check if there is an intersection
	const float a = lineSegment.Dot(lineSegment);
	const float b = 2.0f * circleToLine.Dot(lineSegment);
	const float c = circleToLine.Dot(circleToLine) - radiusSq;

	float discriminant = b * b - 4 * a * c;
	float isIntersection = min(discriminant, a);

	if (isIntersection >= 0.0f)
	{
		// Find exact intersection points (assuming infinite length line)
		discriminant = sqrtf(discriminant);
		float inv2A = 1.0f / (a + a);

		intersectA = (-b + discriminant) * inv2A;
		intersectB = (-b - discriminant) * inv2A;

		// Found if either value valid
		if (intersectA >= 0.0f && intersectA <= 1.0f)
		{
			++numIntersects;
		}

		if (intersectB >= 0.0f && intersectB <= 1.0f)
		{
			++numIntersects;
		}
	}

	return numIntersects;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PointInPolygon2D
// Desc: Determines if a point is in the specified polygon
// Note: Taken from "Cry_GeoOverlap.h" and cut down for our specific tests
//--------------------------------------------------------------------------------------------------
bool PointInPolygon2D(const Vec2& pt, const Vec2* pPolygon, const int numPts)
{
	bool count = false;

	if (pPolygon)
	{
		for (int i = 0, j = 1; i < numPts; ++i, ++j)
		{
			j = (j == numPts) ? 0 : j;

			const Vec2& l0 = pPolygon[i];
			const Vec2& l1 = pPolygon[j];

			if ((((l1.y < pt.y) && (pt.y < l0.y)) || ((l0.y < pt.y) && (pt.y < l1.y)))
			    && (pt.x < (l0.x - l1.x) * (pt.y - l1.y) / (l0.y - l1.y) + l1.x))
			{
				count = !count;
			}
		}
	}

	return count;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculatePolygonArea2D
// Desc: Calculates the area covered by a 2D polygon
//--------------------------------------------------------------------------------------------------
float CalculatePolygonArea2D(const Vec2* pPolygon, const int numPts)
{
	float area = 0.0f;

	if (pPolygon && numPts > 0)
	{
		for (int i = numPts - 1, j = 0; j < numPts; i = j++)
		{
			area += pPolygon[i].x * pPolygon[j].y;
			area -= pPolygon[j].x * pPolygon[i].y;
		}
	}

	return area * 0.5f;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculatePolygonBounds2D
// Desc: Calculates the AA size of a 2D polygon
//--------------------------------------------------------------------------------------------------
Vec2 CalculatePolygonBounds2D(const Vec2* pPolygon, const int numPts)
{
	Vec2 bounds(Vec2Constants<float>::fVec2_Zero);

	if (pPolygon && numPts > 0)
	{
		Vec2 minPt = pPolygon[0], maxPt = pPolygon[0];

		for (int i = 1; i < numPts; ++i)
		{
			minPt.x = min(minPt.x, pPolygon[i].x);
			minPt.y = min(minPt.y, pPolygon[i].y);

			maxPt.x = max(maxPt.x, pPolygon[i].x);
			maxPt.y = max(maxPt.y, pPolygon[i].y);
		}

		bounds = maxPt - minPt;
	}

	return bounds;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculatePolygonCenter2D
// Desc: Calculates the center of a 2D polygon
//--------------------------------------------------------------------------------------------------
Vec2 CalculatePolygonCenter2D(const Vec2* pPolygon, const int numPts)
{
	Vec2 center(0.0f, 0.0f);

	if (pPolygon && numPts > 0)
	{
		const float size = (float)numPts;

		for (int i = 0; i < numPts; ++i)
		{
			center += pPolygon[i];
		}

		center /= size;
	}

	return center;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalculatePolygonSharedPerimeter2D
// Desc: Calculates the connected percentage of fragments
// Note: Returns fraction of perimeter connected in two float params
//--------------------------------------------------------------------------------------------------
bool CalculatePolygonSharedPerimeter2D(const Vec2* pPolygonA, const int numPtsA, const Vec2* pPolygonB, const int numPtsB, float& connectionA, float& connectionB)
{
	// Default values
	const int minPolySize = 3;
	connectionA = connectionB = 0.0f;

	ASSERT_NUM_POLY_SIDES(numPtsA);
	ASSERT_NUM_POLY_SIDES(numPtsB);

	if (pPolygonA && numPtsA >= minPolySize && numPtsA <= POLY_ARRAY_SIZE
	    && pPolygonB && numPtsB >= minPolySize && numPtsB <= POLY_ARRAY_SIZE)
	{
		// Check for shared polygon points
		TPolyIndexArray polyAShared, polyBShared;

		for (int i = 0; i < numPtsA; ++i)
		{
			for (int j = 0; j < numPtsB; ++j)
			{
				// Store matching points
				if (pPolygonA[i] == pPolygonB[j])
				{
					polyAShared.push_back(i);
					polyBShared.push_back(j);
				}
			}
		}

		// Valid connection found when two or more points shared
		const bool connFound = (polyAShared.size() >= 2);

		// Calculate fraction of A that is shared
		if (connFound)
		{
			// Calculate squared length of A's perimeter shared
			const uint polyASize = polyAShared.size() - 1;

			for (uint i = 0; i < polyASize; ++i)
			{
				const Vec2& pt = pPolygonA[polyAShared[i]];
				const Vec2& nextPt = pPolygonA[polyAShared[i + 1]];

				connectionA += (nextPt - pt).GetLength2();
			}

			// Calculate remaining squared perimeter length
			float polyAPerimSq = connectionA;
			int polyIndex = polyAShared.back();

			while (polyIndex != polyAShared[0])
			{
				polyIndex = (polyIndex + 1 == numPtsA) ? 0 : polyIndex + 1;
				int nextPolyIndex = (polyIndex + 1 == numPtsA) ? 0 : polyIndex + 1;

				const Vec2& pt = pPolygonA[polyIndex];
				const Vec2& nextPt = pPolygonA[nextPolyIndex];

				polyAPerimSq += (nextPt - pt).GetLength2();
			}

			// Convert to fraction
			connectionA = sqrtf(connectionA / polyAPerimSq);
		}

		// Calculate fraction of B that is shared
		if (connFound)
		{
			// Calculate squared length of B's perimeter shared
			const uint polyBSize = polyBShared.size() - 1;

			for (uint i = 0; i < polyBSize; ++i)
			{
				const Vec2& pt = pPolygonB[polyBShared[i]];
				const Vec2& nextPt = pPolygonB[polyBShared[i + 1]];

				connectionB += (nextPt - pt).GetLength2();
			}

			// Calculate remaining squared perimeter length
			float polyBPerimSq = connectionB;
			int polyIndex = polyBShared.back();

			while (polyIndex != polyBShared[0])
			{
				polyIndex = (polyIndex + 1 == numPtsB) ? 0 : polyIndex + 1;
				int nextPolyIndex = (polyIndex + 1 == numPtsB) ? 0 : polyIndex + 1;

				const Vec2& pt = pPolygonB[polyIndex];
				const Vec2& nextPt = pPolygonB[nextPolyIndex];

				polyBPerimSq += (nextPt - pt).GetLength2();
			}

			// Convert to fraction
			connectionB = sqrtf(connectionB / polyBPerimSq);
		}

		// Successful
		return true;
	}

	return false;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: TriangulatePolygon2D
// Desc: Generates triangles from a polygon's in-order point list
//--------------------------------------------------------------------------------------------------
void TriangulatePolygon2D(const Vec2* pPolygon, const int numPolyPts, PodArray<Vec2>* pVertices, PodArray<uint8>* pIndices)
{
	ASSERT_NUM_POLY_SIDES(numPolyPts);

	if (pPolygon && numPolyPts >= 3 && numPolyPts <= POLY_ARRAY_SIZE && (pVertices || pIndices))
	{
		int numPts = numPolyPts;
		int numTris = numPts - 2;

		// Offset for output vertex data
		int vertOffset = 0;

		if (pVertices)
		{
			vertOffset = pVertices->Count();
			pVertices->resize(vertOffset + numTris * 3);
		}

		// Offset for output index data
		int indOffset = 0;

		if (pIndices)
		{
			indOffset = pIndices->Count();
			pIndices->resize(indOffset + numTris * 3);
		}

		// Simple case, push single triangle
		if (numTris == 1)
		{
			if (pVertices)
			{
				(*pVertices)[vertOffset] = pPolygon[0];
				(*pVertices)[vertOffset + 1] = pPolygon[1];
				(*pVertices)[vertOffset + 2] = pPolygon[2];
			}

			if (pIndices)
			{
				(*pIndices)[indOffset] = 0;
				(*pIndices)[indOffset + 1] = 1;
				(*pIndices)[indOffset + 2] = 2;
			}
		}
		// Complex case, need to triangulate full point list
		else if (numTris > 1)
		{
			// Initialise outline list as pIndices
			TPolyIndexArray outline;
			outline.resize(numPts);

			for (int i = 0; i < numPts; ++i)
			{
				outline[i] = i;
			}

			// Already know the expected number of triangles
			for (int i = 0; i < numTris; ++i)
			{
				int* pIterBegin = outline.begin();
				int* pIterEnd = outline.end();

				int* pIterA = pIterBegin;
				int* pIterB = pIterBegin;
				int* pIterC = pIterBegin;
				++pIterB;
				++pIterC;
				++pIterC;

				// Loop around entire list and check for ears
				for (int j = 0; j < numPts; ++j)
				{
					const Vec2& a = pPolygon[*pIterA];
					const Vec2& b = pPolygon[*pIterB];
					const Vec2& c = pPolygon[*pIterC];

					// Valid ear triangles will be convex (internal angle < 180)
					if (TriangleIsConvex2D(a, b, c))
					{
						// Check valid triangles against all other outline points
						int* pIterPt = pIterBegin;
						bool isEar = true;

						do
						{
							if (pIterPt != pIterA && pIterPt != pIterB && pIterPt != pIterC)
							{
								const Vec2& pt = pPolygon[*pIterPt];

								// When we have a point in the triangle, it means this is *not* an ear
								if (PointInTriangle2D(pt, a, b, c))
								{
									isEar = false;
									break;
								}
							}

							++pIterPt;
						}
						while (pIterPt != pIterEnd);

						// Create triangle and clip ear when found
						if (isEar)
						{
							if (pVertices)
							{
								(*pVertices)[vertOffset] = pPolygon[*pIterA];
								(*pVertices)[vertOffset + 1] = pPolygon[*pIterB];
								(*pVertices)[vertOffset + 2] = pPolygon[*pIterC];
							}

							if (pIndices)
							{
								(*pIndices)[indOffset] = *pIterA;
								(*pIndices)[indOffset + 1] = *pIterB;
								(*pIndices)[indOffset + 2] = *pIterC;
							}

							// If ear is last point, erase immediately
							if (*pIterB == *pIterEnd)
							{
								outline.pop_back();
							}
							// May need to shuffle points along (rather than erase)
							else
							{
								while (pIterC != pIterEnd)
								{
									*pIterB = *pIterC;
									++pIterB;
									++pIterC;

									// Need to handle looping case
									if (pIterB == pIterEnd)
									{
										pIterB = pIterBegin;
									}
								}

								outline.pop_back();
							}

							// Done with this ear
							break;
						}
					}

					// Try next triangle
					++pIterA;
					++pIterB;
					++pIterC;

					// Treat outline as a ring buffer
					pIterBegin = outline.begin();
					pIterEnd = outline.end();

					if (pIterC == pIterEnd)
					{
						pIterC = pIterBegin;
					}
					else if (pIterB == pIterEnd)
					{
						pIterB = pIterBegin;
					}
				}

				// Clipped one ear point
				--numPts;
				vertOffset += 3;
				indOffset += 3;
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: FindPolygonCircleSplitPoints
// Desc: Common code to finds the split points for SplitPolygonAroundPoint() varations
//--------------------------------------------------------------------------------------------------
struct SPolyCircleSplit
{
	int  A, B, ANext, BNext, numPtsToMove;
	Vec2 ptA, ptB, ptMid;
	bool allPtsInsideCircle, validSegment, cyclicSegment;
};

bool FindPolygonCircleSplitPoints(const Vec2* pPolygon, const int numPts, const Vec2& splitPt, const float splitRadius, SPolyCircleSplit& split)
{
	// Default data
	const int invalidPt = -1;
	split.A = split.B = invalidPt;
	split.allPtsInsideCircle = true;

	if (pPolygon && numPts >= 3)
	{
		const float splitRadiusSq = splitRadius * splitRadius;
		float intersectA, intersectB;

		// Loop round polygon
		for (int i = 0; i < numPts; ++i)
		{
			// Check segment for intersection
			int j = (i + 1 == numPts) ? 0 : i + 1;

			float tA, tB;
			int numIntersects = LineCircleIntersect2D(pPolygon[i], pPolygon[j], splitPt, splitRadiusSq, tA, tB);

			// Store line intersection points if found
			if (numIntersects == 1)
			{
				tA = (tA<0.0f || tA> 1.0f) ? tB : tA;

				if (split.A == invalidPt)
				{
					split.A = i;
					intersectA = tA;
				}
				else
				{
					split.B = i;
					intersectB = tA;
					break;
				}
			}
			// Can be a case where the circle intersects fully on one line. This
			// means we can't really split the polygon though, so early out
			else if (numIntersects == 2)
			{
				split.A = split.B = invalidPt;
				intersectA = intersectB = 0.0f;
				break;
			}

			// Check if we still have all points inside the circle
			if (split.allPtsInsideCircle)
			{
				const float distSq = (pPolygon[i] - splitPt).GetLength2();

				if (distSq > splitRadiusSq)
				{
					split.allPtsInsideCircle = false;
				}
			}
		}

		// Valid polygon segment?
		if (split.validSegment = (split.A >= 0 && split.B >= 0 && split.A != split.B))
		{
			// May need to swap points to ensure generating inner polygon
			const float splitADistSq = (pPolygon[split.A] - splitPt).GetLength2();

			if (splitADistSq < splitRadiusSq)
			{
				int temp = split.A;
				split.A = split.B;
				split.B = temp;
			}

			// Calculate how many points in cut segment
			split.cyclicSegment = split.A > split.B;
			split.numPtsToMove = split.cyclicSegment ? (split.B + numPts) - split.A : split.B - split.A;

			if (split.numPtsToMove > 0)
			{
				// Calculate intersect points
				split.ANext = (split.A + 1 == numPts) ? 0 : split.A + 1;
				split.BNext = (split.B + 1 == numPts) ? 0 : split.B + 1;

				split.ptA = pPolygon[split.A] * intersectA + pPolygon[split.ANext] * (1.0f - intersectA);
				split.ptB = pPolygon[split.B] * intersectB + pPolygon[split.BNext] * (1.0f - intersectB);

				// Extra mid-point lying on circle, for use with the outer polygon only
				split.ptMid = (split.ptA + split.ptB) * 0.5f;
				split.ptMid = splitPt + (split.ptMid - splitPt).GetNormalized() * splitRadius;
			}
			else
			{
				split.validSegment = false;
			}
		}
	}

	return (split.validSegment && split.numPtsToMove > 0);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SplitPolygonAroundPoint <TPolygonArray>
// Desc: Splits a polygon outline along the radial line around the point
// Note: Removes verts from input data, then creates the INNER polygon piece.
//			 Does not do anything if polygon fully contains circle
//--------------------------------------------------------------------------------------------------
void SplitPolygonAroundPoint(TPolygonArray& outline, const Vec2& splitPt, const float splitRadius, TPolygonArray& splitInnerOutline)
{
	const Vec2* pOutlinePts = outline.begin();
	const int outlineSize = outline.size();

	// Build inner polygon if both split points where found, and they create a valid segment
	SPolyCircleSplit split;
	if (FindPolygonCircleSplitPoints(pOutlinePts, outlineSize, splitPt, splitRadius, split))
	{
		// Create new inner polygon from split points and old polygon segment (if arrays large enough)
		if (split.numPtsToMove > 0 && split.numPtsToMove <= POLY_ARRAY_SIZE - 2)
		{
			splitInnerOutline.resize(split.numPtsToMove + 2);

			splitInnerOutline[0] = split.ptA;
			splitInnerOutline[split.numPtsToMove + 1] = split.ptB;

			for (int i = 1; i < split.numPtsToMove + 1; ++i)
			{
				int j = split.A + i;
				j = (j >= outlineSize) ? j - outlineSize : j;

				splitInnerOutline[i] = pOutlinePts[j];
			}
		}
		else
		{
			splitInnerOutline.clear();
		}

		// Cyclic segment - need to delete both ends, then add intersect points
		if (split.cyclicSegment)
		{
			const int numStartPts = split.B + 1;
			const int numEndPts = outlineSize - (split.A + 1);
			const bool canAddMidPt = (numStartPts + numEndPts >= 3);

			if (numEndPts > 0)
			{
				outline.erase(split.A + 1, numEndPts);
			}

			if (numStartPts > 0)
			{
				outline.erase(0, numStartPts);
			}

			outline.push_back(split.ptA);
			if (canAddMidPt)
			{
				outline.push_back(split.ptMid);
			}
			outline.push_back(split.ptB);
		}
		// Contiguous segment - single delete then insert intersect points
		else
		{
			outline.erase(split.ANext, split.numPtsToMove);

			const bool splitTouchesEnd = (split.B == outlineSize - 1);
			const bool canAddMidPt = (split.numPtsToMove >= 3);
			if (splitTouchesEnd)
			{
				outline.push_back(split.ptA);
				if (canAddMidPt)
				{
					outline.push_back(split.ptMid);
				}
				outline.push_back(split.ptB);
			}
			else
			{
				outline.insert_before(split.ptB, split.A + 1);
				if (canAddMidPt)
				{
					outline.insert_before(split.ptMid, split.A + 1);
				}
				outline.insert_before(split.ptA, split.A + 1);
			}
		}
	}
	// If failed, might be the case where entire polygon is inside
	else if (split.allPtsInsideCircle)
	{
		// Move the polygon to the output list
		splitInnerOutline.resize(outlineSize);

		for (int i = 0; i < outlineSize; ++i)
		{
			splitInnerOutline[i] = pOutlinePts[i];
		}

		outline.clear();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SplitPolygonAroundPoint <PodArray>
// Desc: Splits a polygon outline along the radial line around the point
// Note: Removes verts from input data, then creates the INNER polygon piece.
//			 Does not do anything if polygon fully contains circle
//--------------------------------------------------------------------------------------------------
void SplitPolygonAroundPoint(PodArray<Vec2>& outline, const Vec2& splitPt, const float splitRadius, TPolygonArray& splitInnerOutline)
{
	const Vec2* pOutlinePts = outline.begin();
	const int outlineSize = outline.Count();

	// Build inner polygon if both split points where found, and they create a valid segment
	SPolyCircleSplit split;
	if (FindPolygonCircleSplitPoints(pOutlinePts, outlineSize, splitPt, splitRadius, split))
	{
		// Create new inner polygon from split points and old polygon segment
		splitInnerOutline.resize(split.numPtsToMove + 2);

		splitInnerOutline[0] = split.ptA;
		splitInnerOutline[split.numPtsToMove + 1] = split.ptB;

		for (int i = 1; i < split.numPtsToMove + 1; ++i)
		{
			int j = split.A + i;
			j = (j >= outlineSize) ? j - outlineSize : j;

			splitInnerOutline[i] = pOutlinePts[j];
		}

		// Cyclic segment - need to delete both ends, then add intersect points
		if (split.cyclicSegment)
		{
			const int numStartPts = split.B + 1;
			const int numEndPts = outlineSize - (split.A + 1);

			if (numEndPts > 0)
			{
				outline.Delete(split.A + 1, numEndPts);
			}

			if (numStartPts > 0)
			{
				outline.Delete(0, numStartPts);
			}

			outline.push_back(split.ptA);
			outline.push_back(split.ptMid);
			outline.push_back(split.ptB);
		}
		// Contiguous segment - single delete then insert intersect points
		else
		{
			outline.Delete(split.ANext, split.numPtsToMove);

			const bool splitTouchesEnd = split.B == outlineSize - 1;
			if (splitTouchesEnd)
			{
				outline.push_back(split.ptA);
				outline.push_back(split.ptMid);
				outline.push_back(split.ptB);
			}
			else
			{
				outline.InsertBefore(split.ptB, split.A + 1);
				outline.InsertBefore(split.ptMid, split.A + 1);
				outline.InsertBefore(split.ptA, split.A + 1);
			}
		}
	}
	// If failed, might be the case where entire polygon is inside
	else if (split.allPtsInsideCircle)
	{
		// Move the polygon to the output list
		splitInnerOutline.resize(outlineSize);

		for (int i = 0; i < outlineSize; ++i)
		{
			splitInnerOutline[i] = pOutlinePts[i];
		}

		outline.Clear();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PolygonInCircle2D
// Desc: Determines if a polygon is contained within the specified circle
//--------------------------------------------------------------------------------------------------
EPolygonInCircle2D PolygonInCircle2D(const Vec2& center, const float radius, const Vec2* pPolygon, const int numPts)
{
	EPolygonInCircle2D state = EPolygonInCircle2D_Outside;

	if (pPolygon && numPts >= 3)
	{
		const float radiusSq = radius * radius;

		Vec2 centerDir;
		float centerDistSq;
		int numPtsOutside = 0;

		// Check state of each point
		for (int i = 0; i < numPts; ++i)
		{
			centerDir = center - pPolygon[i];
			centerDistSq = centerDir.GetLength2();

			if (centerDistSq > radiusSq)
			{
				++numPtsOutside;
			}
		}

		// Determine state
		if (numPtsOutside == numPts)
		{
			state = EPolygonInCircle2D_Outside;
		}
		else if (numPtsOutside > 0)
		{
			state = EPolygonInCircle2D_Overlap;
		}
		else
		{
			state = EPolygonInCircle2D_Inside;
		}
	}

	return state;
}//-------------------------------------------------------------------------------------------------
