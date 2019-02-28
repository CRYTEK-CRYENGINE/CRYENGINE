// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _POLYGON_MATH_2D_
#define _POLYGON_MATH_2D_
#pragma once

#include <CryCore/Containers/CryFixedArray.h>

//==================================================================================================
// Name: FixedPodArray
// Desc: Extension of CryFixedArray to add useful functionality
// Note: This should be promoted into CryFixedArray in the long term
// Author: Chris Bunner
//==================================================================================================
template<class T, unsigned int N>
class FixedPodArray : public CryFixedArray<T, N>
{
	typedef CryFixedArray<T, N> CFA;

public:
	ILINE void resize(const uint size)
	{
		if (size <= N)
		{
			CFA::m_curSize[0] = size;
		}
		else
		{
			CryLogAlways("FixedPodArray::resize() failing as size too large - NOT resizing array");
			CRY_ASSERT_MESSAGE(0, "FixedPodArray::resize() failing as size too large - NOT resizing array");
		}
	}

	ILINE void erase(const uint index, const uint count = 1)
	{
		const uint size = CFA::m_curSize[0];

		if (index == size - count)
		{
			// Simple remove
			CFA::m_curSize[0] -= count;
		}
		else if (index + count < size)
		{
			// Shuffle set of elements along
			const uint range = size - (index + count);
			memmove(&CFA::at(index), &CFA::at(index + count), sizeof(T) * range);

			CFA::m_curSize[0] -= count;
		}
		else
		{
			CryLogAlways("FixedPodArray::erase() failing as element range invalid - NOT removing element");
			CRY_ASSERT_MESSAGE(0, "FixedPodArray::erase() failing as element range invalid - NOT removing element");
		}
	}

	ILINE void insert_before(const T& elem, const uint index)
	{
		const uint size = CFA::m_curSize[0];

		if (index < size && size < N)
		{
			// Shuffle set of elements along
			const uint count = size - index;
			memmove(&CFA::at(index + 1), &CFA::at(index), sizeof(T) * count);

			// Add new element
			memcpy(&CFA::at(index), &elem, sizeof(T));
			++CFA::m_curSize[0];
		}
		else
		{
			CryLogAlways("FixedPodArray::insert_before() failing as element index invalid - NOT inserting element");
			CRY_ASSERT_MESSAGE(0, "FixedPodArray::insert_before() failing as element index invalid - NOT inserting element");
		}
	}

	ILINE int find(const T& elem)
	{
		const uint size = CFA::m_curSize[0];
		const int invalid = -1;

		int index = invalid;

		for (uint i = 0; i < size; ++i)
		{
			if (CFA::at(i) == elem)
			{
				index = i;
				break;
			}
		}

		return index;
	}
};

//==================================================================================================
// Name: PolygonMath2D
// Desc: Polygon math helper functions optimized for breakable glass sim
// Note: Functions often assume clockwise polygon point order
// Author: Chris Bunner
//==================================================================================================

// Common polygon array types
#define POLY_ARRAY_SIZE 45

typedef FixedPodArray<Vec2, POLY_ARRAY_SIZE> TPolygonArray;
typedef FixedPodArray<int, POLY_ARRAY_SIZE>  TPolyIndexArray;

// 2D triangle helpers
bool PointInTriangle2D(const Vec2& pt, const Vec2& a, const Vec2& b, const Vec2& c);
bool TriangleIsConvex2D(const Vec2& a, const Vec2& b, const Vec2& c);

// 2D intersection helpers
int LineCircleIntersect2D(const Vec2& pt0, const Vec2& pt1, const Vec2& center, const float radiusSq, float& intersectA, float& intersectB);

// 2D polygon helpers
bool  PointInPolygon2D(const Vec2& pt, const Vec2* pPolygon, const int numPts);

float CalculatePolygonArea2D(const Vec2* pPolygon, const int numPts);
Vec2  CalculatePolygonBounds2D(const Vec2* pPolygon, const int numPts);
Vec2  CalculatePolygonCenter2D(const Vec2* pPolygon, const int numPts);
bool  CalculatePolygonSharedPerimeter2D(const Vec2* pPolygonA, const int numPtsA, const Vec2* pPolygonB, const int numPtsB, float& connectionA, float& connectionB);

void  TriangulatePolygon2D(const Vec2* pPolygon, const int numPolyPts, PodArray<Vec2>* pVertices, PodArray<uint8>* pIndices);
void  SplitPolygonAroundPoint(TPolygonArray& outline, const Vec2& splitPt, const float splitRadius, TPolygonArray& splitInnerOutline);
void  SplitPolygonAroundPoint(PodArray<Vec2>& outline, const Vec2& splitPt, const float splitRadius, TPolygonArray& splitInnerOutline);

// Helper with result enum
enum EPolygonInCircle2D
{
	EPolygonInCircle2D_Inside = 0,
	EPolygonInCircle2D_Outside,
	EPolygonInCircle2D_Overlap
};

EPolygonInCircle2D PolygonInCircle2D(const Vec2& center, const float radius, const Vec2* pPolygon, const int numPts);

#endif // _POLYGON_MATH_2D_
