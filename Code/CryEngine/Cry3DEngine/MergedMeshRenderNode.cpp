// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <numeric>
#include <algorithm>
#include <unordered_map>

#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_GeoIntersect.h>
#include <CryThreading/IJobManager_JobDelegator.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryAnimation/ICryAnimation.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryMath/QTangent.h>
#include <CryString/StringUtils.h>

#include "MergedMeshRenderNode.h"
#include "MergedMeshGeometry.h"
#include "DeformableNode.h"

#define FSL_CREATE_MODE FSL_VIDEO_CREATE

#include <CryCore/StlUtils.h>

namespace
{
static IGeneralMemoryHeap* s_MergedMeshPool = NULL;
static CryCriticalSection s_MergedMeshPoolLock;

static inline SMeshBoneMapping_uint8 _SortBoneMapping(const SMeshBoneMapping_uint8& a)
{
	SMeshBoneMapping_uint8 r;
	memset(&r, 0, sizeof(r));
	for (size_t i = 0, j = 0; i < 4; ++i)
	{
		if (a.weights[i])
		{
			r.weights[j] = a.weights[i];
			r.boneIds[j] = a.boneIds[i];
			++j;
		}
	}
	return r;
}
static inline bool operator==(const SMeshBoneMapping_uint8& a, const SMeshBoneMapping_uint8& b)
{
	const SMeshBoneMapping_uint8 _a = _SortBoneMapping(a);
	const SMeshBoneMapping_uint8 _b = _SortBoneMapping(b);
	return memcmp(&_a, &_b, sizeof(_a)) == 0;
}

static inline bool operator!=(const SMeshBoneMapping_uint8& a, const SMeshBoneMapping_uint8& b)
{
	return !(a == b);
}

// A hash function with multiplier 65599 (from Red Dragon book)
static inline size_t hasher(const char* s, size_t len)
{
	size_t hash = 0;
	for (size_t i = 0; i < len; ++i)
		hash = 65599 * hash + s[i];
	return hash ^ (hash >> 16);
}

#if MMRM_RENDER_DEBUG && MMRM_VISUALIZE_WINDFIELD
void VisualizeWindField(const Vec3* samples, const AABB& bbox, bool wind_dir, bool trace)
{
	IRenderAuxGeom* pAuxGeomRender = gEnv->pRenderer->GetIRenderAuxGeom();
	pAuxGeomRender->DrawAABB(bbox, false, Col_Green, eBBD_Faceted);
	const Vec3& size = bbox.GetSize();
	if (wind_dir)
	{
		for (size_t x = 0; x < MMRM_WIND_DIM; ++x)
			for (size_t y = 0; y < MMRM_WIND_DIM; ++y)
				for (size_t z = 0; z < MMRM_WIND_DIM; ++z)
				{
					Vec3 pos, w;
					pos.x = bbox.min.x + ((float)x / (float)((MMRM_WIND_DIM))) * size.x;
					pos.y = bbox.min.y + ((float)y / (float)((MMRM_WIND_DIM))) * size.y;
					pos.z = bbox.min.z + ((float)z / (float)((MMRM_WIND_DIM))) * size.z;
					pAuxGeomRender->DrawLine(pos, Col_Green, pos + samples[x + y * MMRM_WIND_DIM + z * sqr(MMRM_WIND_DIM)].GetNormalized(), Col_Pink, 1.f);
				}
	}
	if (trace)
	{
		std::vector<std::pair<Vec3, Vec3>> points;
		const size_t count = MMRM_WIND_DIM * 4;
		for (size_t x = 0; x < count; ++x)
			for (size_t y = 0; y < count; ++y)
				for (size_t z = 0; z < count; ++z)
				{
					Vec3 pos;
					pos.x = ((float)x / (float)(count));
					pos.y = ((float)y / (float)(count));
					pos.z = ((float)z / (float)(count));
					points.push_back(std::make_pair(pos, Vec3(0, 0, 0)));
				}
		for (size_t i = 0; i < 16; ++i)
		{
			ColorB col0;
			col0.lerpFloat(Col_Blue, Col_Red, i / 16.f);
			ColorB col1;
			col1.lerpFloat(Col_Blue, Col_Red, (i + 1) / 16.f);
			for (size_t j = 0; j < count * count * count; ++j)
			{
				const Vec3& pos = points[j].first;
				const Vec3& vel = points[j].second;
				Vec3 realpos;
				realpos.x = bbox.min.x + pos.x * size.x;
				realpos.y = bbox.min.y + pos.y * size.y;
				realpos.z = bbox.min.z + pos.z * size.z;

				const Vec3& nvel = vel + ::SampleWind(pos, samples) * (1.f / 8.f);
				Vec3 npos = realpos + nvel * 0.3333f;
				pAuxGeomRender->DrawLine(realpos, col0, npos, col1, 1.f);

				npos.x = (realpos.x - bbox.min.x) / size.x;
				npos.y = (realpos.y - bbox.min.y) / size.y;
				npos.z = (realpos.z - bbox.min.z) / size.z;
				points[j].first = npos;
				points[j].second = nvel;
			}
		}
	}
}
#endif

// Three points are a counter-clockwise turn if turn() > 0, clockwise if
// turn() < 0, and collinear if turn() == 0.
static inline float turn(const Vec2& p1, const Vec2& p2, const Vec2& p3)
{
	return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
}

// Weirdo functions for DynArray
// NOTE: Can't make these static, gcc wants an external for a template parameter in graham_reduce
inline void graham_increment(Vec2*& iter) { ++iter; }
inline void graham_decrement(Vec2*& iter) { --iter; }

// Create and maintain a convex hull while iterating over a sorted list of points
template<void(Advance)(Vec2*&)>
static size_t graham_reduce(Vec2* begin, Vec2* end, DynArray<Vec2>& hull)
{
	while (begin != end)
	{
		while (hull.size() > 1 && turn(*(hull.rbegin() - 1), *(hull.rbegin()), *begin) > 0)
			hull.pop_back();
		if (!hull.size() || *hull.rbegin() != *begin)
			hull.push_back(*begin);
		Advance(begin);
	}
	return hull.size();
}

static inline bool vec_compare_lex(const Vec2& a, const Vec2& b)
{
	if (a.x < b.x)
		return true;
	if (a.x > b.x)
		return false;
	return a.y < b.y;
}

// Basic Graham scan implementation. Note this variant does not sort the points by
// polar angle but rather performs the ccw-reduction pass on the sorted point list
// twice, once in regular and once in reversed order producing the upper and lower
// convex hull which get merged as a last step.
//
// The original algorithm (as can be seen on wikipedia) calculates the hull
// in-place in the input list, but has to deal with searching for the lowest
// y-coordinate and sorting the input list by polar angles of each point in respect to the
// x-axis, so (apart from the additional allocations) the running time should be roughly
// identical.
static size_t convexhull_graham_scan(DynArray<Vec2>& points)
{
	// If we have less than 4 points, we already have our hull implicitly
	if (points.size() < 4)
		return points.size();

	// Sort the point list by x-coordinate
	std::sort(points.begin(), points.end(), vec_compare_lex);
	Vec2* begin = points.begin();
	Vec2* end = std::unique(begin, points.end());

	// Calculate the lower and upper half of the hull
	DynArray<Vec2> lower, upper;
	graham_reduce<graham_increment>(begin, end, lower);
	graham_reduce<graham_decrement>(end - 1, begin - 1, upper);

	// Merge the two hulls and swap it with input array
	// Note: it's safe to skip the first and the last points as they will
	// in lower already

	// Note: The insert method of our moronic dynarray<T> DOES NOT COMPILE!
	for (size_t i = 1, end = upper.size() - 1; i < end; lower.push_back(upper[i++]))
		;

	std::swap(points, lower);
	return points.size();
};

static size_t convexhull_giftwrap(DynArray<Vec2>& points)
{
	size_t i = 0, h = 0, n = points.size();
	for (size_t j = 0; j < n; ++j)
		if (points[j].y < points[i].y)
			i = j;
	do
	{
		std::swap(points[i], points[h]);
		for (size_t j = 0; j < n; ++j)
			if (turn(points[h], points[i], points[j]) < 0)
				i = j;
		h++;
	}
	while (i > 0);

	points.erase(points.begin() + h, points.end());
	return points.size();
}

// *INDENT-OFF* - To keep the cube in the comment unchanged
    /*
     int edgeTable[256].  It corresponds to the 2^8 possible combinations of
     of the eight (n) vertices either existing inside or outside (2^n) of the
     surface.  A vertex is inside of a surface if the value at that vertex is
     less than that of the surface you are scanning for.  The table index is
     constructed bitwise with bit 0 corresponding to vertex 0, bit 1 to vert
     1.. bit 7 to vert 7.  The value in the table tells you which edges of
     the table are intersected by the surface.  Once again bit 0 corresponds
     to edge 0 and so on, up to edge 12.
     Constructing the table simply consisted of having a program run thru
     the 256 cases and setting the edge bit if the vertices at either end of
     the edge had different values (one is inside while the other is out).
     The purpose of the table is to speed up the scanning process.  Only the
     edges whose bit's are set contain vertices of the surface.
     Vertex 0 is on the bottom face, back edge, left side.
     The progression of vertices is clockwise around the bottom face
     and then clockwise around the top face of the cube.  Edge 0 goes from
     vertex 0 to vertex 1, Edge 2 is from 2->3 and so on around clockwise to
     vertex 0 again. Then Edge 4 to 7 make up the top face, 4->5, 5->6, 6->7
     and 7->4.  Edge 8 thru 11 are the vertical edges from vert 0->4, 1->5,
     2->6, and 3->7.
         4--------5     *---4----*
        /|       /|    /|       /|
       / |      / |   7 |      5 |
      /  |     /  |  /  8     /  9
     7--------6   | *----6---*   |
     |   |    |   | |   |    |   |
     |   0----|---1 |   *---0|---*
     |  /     |  /  11 /     10 /
     | /      | /   | 3      | 1
     |/       |/    |/       |/
     3--------2     *---2----*
  */
// *INDENT-ON*

static uint16 s_marching_cubes_edge_table[256] =
{
	0x0,   0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
	0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
	0x190, 0x99,  0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
	0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
	0x230, 0x339, 0x33,  0x13a, 0x636, 0x73f, 0x435, 0x53c,
	0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
	0x3a0, 0x2a9, 0x1a3, 0xaa,  0x7a6, 0x6af, 0x5a5, 0x4ac,
	0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
	0x460, 0x569, 0x663, 0x76a, 0x66,  0x16f, 0x265, 0x36c,
	0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
	0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff,  0x3f5, 0x2fc,
	0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
	0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55,  0x15c,
	0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
	0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc,
	0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
	0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
	0xcc,  0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
	0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
	0x15c, 0x55,  0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
	0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
	0x2fc, 0x3f5, 0xff,  0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
	0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
	0x36c, 0x265, 0x16f, 0x66,  0x76a, 0x663, 0x569, 0x460,
	0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
	0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa,  0x1a3, 0x2a9, 0x3a0,
	0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
	0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33,  0x339, 0x230,
	0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
	0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99,  0x190,
	0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
	0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
};

/*
   int8 s_triTable[256][16] also corresponds to the 256 possible combinations
   of vertices.
   The [16] dimension of the table is again the list of edges of the cube
   which are intersected by the surface.  This time however, the edges are
   enumerated in the order of the vertices making up the triangle mesh of
   the surface.  Each edge contains one vertex that is on the surface.
   Each triple of edges listed in the table contains the vertices of one
   triangle on the mesh.  The are 16 entries because it has been shown that
   there are at most 5 triangles in a cube and each "edge triple" list is
   terminated with the value -1.
   For example triTable[3] contains
   {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
   This corresponds to the case of a cube whose vertex 0 and 1 are inside
   of the surface and the rest of the verts are outside (00000001 bitwise
   OR'ed with 00000010 makes 00000011 == 3).  Therefore, this cube is
   intersected by the surface roughly in the form of a plane which cuts
   edges 8,9,1 and 3.  This quadrilateral can be constructed from two
   triangles: one which is made of the intersection vertices found on edges
   1,8, and 3; the other is formed from the vertices on edges 9,8, and 1.
   Remember, each intersected edge contains only one surface vertex.  The
   vertex triples are listed in counter clockwise order for proper facing.
 */
static int8 s_marching_cubes_tri_table[256][16] =
{
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  3,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  1,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  8,  3,  9,  8,  1,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  3,  1,  2,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  2,  10, 0,  2,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  8,  3,  2,  10, 8,  10, 9,  8,  -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  11, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  11, 2,  8,  11, 0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  9,  0,  2,  3,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  11, 2,  1,  9,  11, 9,  8,  11, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  10, 1,  11, 10, 3,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  10, 1,  0,  8,  10, 8,  11, 10, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  9,  0,  3,  11, 9,  11, 10, 9,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  8,  10, 10, 8,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  7,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  3,  0,  7,  3,  4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  1,  9,  8,  4,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  1,  9,  4,  7,  1,  7,  3,  1,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  10, 8,  4,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  4,  7,  3,  0,  4,  1,  2,  10, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  2,  10, 9,  0,  2,  8,  4,  7,  -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  10, 9,  2,  9,  7,  2,  7,  3,  7,  9,  4,  -1, -1, -1, -1 },
	{ 8,  4,  7,  3,  11, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 11, 4,  7,  11, 2,  4,  2,  0,  4,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  0,  1,  8,  4,  7,  2,  3,  11, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  7,  11, 9,  4,  11, 9,  11, 2,  9,  2,  1,  -1, -1, -1, -1 },
	{ 3,  10, 1,  3,  11, 10, 7,  8,  4,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  11, 10, 1,  4,  11, 1,  0,  4,  7,  11, 4,  -1, -1, -1, -1 },
	{ 4,  7,  8,  9,  0,  11, 9,  11, 10, 11, 0,  3,  -1, -1, -1, -1 },
	{ 4,  7,  11, 4,  11, 9,  9,  11, 10, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  5,  4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  5,  4,  0,  8,  3,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  5,  4,  1,  5,  0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 8,  5,  4,  8,  3,  5,  3,  1,  5,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  10, 9,  5,  4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  0,  8,  1,  2,  10, 4,  9,  5,  -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  2,  10, 5,  4,  2,  4,  0,  2,  -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  10, 5,  3,  2,  5,  3,  5,  4,  3,  4,  8,  -1, -1, -1, -1 },
	{ 9,  5,  4,  2,  3,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  11, 2,  0,  8,  11, 4,  9,  5,  -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  5,  4,  0,  1,  5,  2,  3,  11, -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  1,  5,  2,  5,  8,  2,  8,  11, 4,  8,  5,  -1, -1, -1, -1 },
	{ 10, 3,  11, 10, 1,  3,  9,  5,  4,  -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  9,  5,  0,  8,  1,  8,  10, 1,  8,  11, 10, -1, -1, -1, -1 },
	{ 5,  4,  0,  5,  0,  11, 5,  11, 10, 11, 0,  3,  -1, -1, -1, -1 },
	{ 5,  4,  8,  5,  8,  10, 10, 8,  11, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  7,  8,  5,  7,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  3,  0,  9,  5,  3,  5,  7,  3,  -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  7,  8,  0,  1,  7,  1,  5,  7,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  5,  3,  3,  5,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  7,  8,  9,  5,  7,  10, 1,  2,  -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 1,  2,  9,  5,  0,  5,  3,  0,  5,  7,  3,  -1, -1, -1, -1 },
	{ 8,  0,  2,  8,  2,  5,  8,  5,  7,  10, 5,  2,  -1, -1, -1, -1 },
	{ 2,  10, 5,  2,  5,  3,  3,  5,  7,  -1, -1, -1, -1, -1, -1, -1 },
	{ 7,  9,  5,  7,  8,  9,  3,  11, 2,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  5,  7,  9,  7,  2,  9,  2,  0,  2,  7,  11, -1, -1, -1, -1 },
	{ 2,  3,  11, 0,  1,  8,  1,  7,  8,  1,  5,  7,  -1, -1, -1, -1 },
	{ 11, 2,  1,  11, 1,  7,  7,  1,  5,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  5,  8,  8,  5,  7,  10, 1,  3,  10, 3,  11, -1, -1, -1, -1 },
	{ 5,  7,  0,  5,  0,  9,  7,  11, 0,  1,  0,  10, 11, 10, 0,  -1 },
	{ 11, 10, 0,  11, 0,  3,  10, 5,  0,  8,  0,  7,  5,  7,  0,  -1 },
	{ 11, 10, 5,  7,  11, 5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 6,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  3,  5,  10, 6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  0,  1,  5,  10, 6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  8,  3,  1,  9,  8,  5,  10, 6,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  6,  5,  2,  6,  1,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  6,  5,  1,  2,  6,  3,  0,  8,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  6,  5,  9,  0,  6,  0,  2,  6,  -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  9,  8,  5,  8,  2,  5,  2,  6,  3,  2,  8,  -1, -1, -1, -1 },
	{ 2,  3,  11, 10, 6,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 11, 0,  8,  11, 2,  0,  10, 6,  5,  -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  1,  9,  2,  3,  11, 5,  10, 6,  -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  10, 6,  1,  9,  2,  9,  11, 2,  9,  8,  11, -1, -1, -1, -1 },
	{ 6,  3,  11, 6,  5,  3,  5,  1,  3,  -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  11, 0,  11, 5,  0,  5,  1,  5,  11, 6,  -1, -1, -1, -1 },
	{ 3,  11, 6,  0,  3,  6,  0,  6,  5,  0,  5,  9,  -1, -1, -1, -1 },
	{ 6,  5,  9,  6,  9,  11, 11, 9,  8,  -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  10, 6,  4,  7,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  3,  0,  4,  7,  3,  6,  5,  10, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  9,  0,  5,  10, 6,  8,  4,  7,  -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 6,  5,  1,  9,  7,  1,  7,  3,  7,  9,  4,  -1, -1, -1, -1 },
	{ 6,  1,  2,  6,  5,  1,  4,  7,  8,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  5,  5,  2,  6,  3,  0,  4,  3,  4,  7,  -1, -1, -1, -1 },
	{ 8,  4,  7,  9,  0,  5,  0,  6,  5,  0,  2,  6,  -1, -1, -1, -1 },
	{ 7,  3,  9,  7,  9,  4,  3,  2,  9,  5,  9,  6,  2,  6,  9,  -1 },
	{ 3,  11, 2,  7,  8,  4,  10, 6,  5,  -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  10, 6,  4,  7,  2,  4,  2,  0,  2,  7,  11, -1, -1, -1, -1 },
	{ 0,  1,  9,  4,  7,  8,  2,  3,  11, 5,  10, 6,  -1, -1, -1, -1 },
	{ 9,  2,  1,  9,  11, 2,  9,  4,  11, 7,  11, 4,  5,  10, 6,  -1 },
	{ 8,  4,  7,  3,  11, 5,  3,  5,  1,  5,  11, 6,  -1, -1, -1, -1 },
	{ 5,  1,  11, 5,  11, 6,  1,  0,  11, 7,  11, 4,  0,  4,  11, -1 },
	{ 0,  5,  9,  0,  6,  5,  0,  3,  6,  11, 6,  3,  8,  4,  7,  -1 },
	{ 6,  5,  9,  6,  9,  11, 4,  7,  9,  7,  11, 9,  -1, -1, -1, -1 },
	{ 10, 4,  9,  6,  4,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  10, 6,  4,  9,  10, 0,  8,  3,  -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 0,  1,  10, 6,  0,  6,  4,  0,  -1, -1, -1, -1, -1, -1, -1 },
	{ 8,  3,  1,  8,  1,  6,  8,  6,  4,  6,  1,  10, -1, -1, -1, -1 },
	{ 1,  4,  9,  1,  2,  4,  2,  6,  4,  -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  0,  8,  1,  2,  9,  2,  4,  9,  2,  6,  4,  -1, -1, -1, -1 },
	{ 0,  2,  4,  4,  2,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 8,  3,  2,  8,  2,  4,  4,  2,  6,  -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 4,  9,  10, 6,  4,  11, 2,  3,  -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  2,  2,  8,  11, 4,  9,  10, 4,  10, 6,  -1, -1, -1, -1 },
	{ 3,  11, 2,  0,  1,  6,  0,  6,  4,  6,  1,  10, -1, -1, -1, -1 },
	{ 6,  4,  1,  6,  1,  10, 4,  8,  1,  2,  1,  11, 8,  11, 1,  -1 },
	{ 9,  6,  4,  9,  3,  6,  9,  1,  3,  11, 6,  3,  -1, -1, -1, -1 },
	{ 8,  11, 1,  8,  1,  0,  11, 6,  1,  9,  1,  4,  6,  4,  1,  -1 },
	{ 3,  11, 6,  3,  6,  0,  0,  6,  4,  -1, -1, -1, -1, -1, -1, -1 },
	{ 6,  4,  8,  11, 6,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 7,  10, 6,  7,  8,  10, 8,  9,  10, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  7,  3,  0,  10, 7,  0,  9,  10, 6,  7,  10, -1, -1, -1, -1 },
	{ 10, 6,  7,  1,  10, 7,  1,  7,  8,  1,  8,  0,  -1, -1, -1, -1 },
	{ 10, 6,  7,  10, 7,  1,  1,  7,  3,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  6,  1,  6,  8,  1,  8,  9,  8,  6,  7,  -1, -1, -1, -1 },
	{ 2,  6,  9,  2,  9,  1,  6,  7,  9,  0,  9,  3,  7,  3,  9,  -1 },
	{ 7,  8,  0,  7,  0,  6,  6,  0,  2,  -1, -1, -1, -1, -1, -1, -1 },
	{ 7,  3,  2,  6,  7,  2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  3,  11, 10, 6,  8,  10, 8,  9,  8,  6,  7,  -1, -1, -1, -1 },
	{ 2,  0,  7,  2,  7,  11, 0,  9,  7,  6,  7,  10, 9,  10, 7,  -1 },
	{ 1,  8,  0,  1,  7,  8,  1,  10, 7,  6,  7,  10, 2,  3,  11, -1 },
	{ 11, 2,  1,  11, 1,  7,  10, 6,  1,  6,  7,  1,  -1, -1, -1, -1 },
	{ 8,  9,  6,  8,  6,  7,  9,  1,  6,  11, 6,  3,  1,  3,  6,  -1 },
	{ 0,  9,  1,  11, 6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 7,  8,  0,  7,  0,  6,  3,  11, 0,  11, 6,  0,  -1, -1, -1, -1 },
	{ 7,  11, 6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 7,  6,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  0,  8,  11, 7,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  1,  9,  11, 7,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 8,  1,  9,  8,  3,  1,  11, 7,  6,  -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 1,  2,  6,  11, 7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  10, 3,  0,  8,  6,  11, 7,  -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  9,  0,  2,  10, 9,  6,  11, 7,  -1, -1, -1, -1, -1, -1, -1 },
	{ 6,  11, 7,  2,  10, 3,  10, 8,  3,  10, 9,  8,  -1, -1, -1, -1 },
	{ 7,  2,  3,  6,  2,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 7,  0,  8,  7,  6,  0,  6,  2,  0,  -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  7,  6,  2,  3,  7,  0,  1,  9,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  6,  2,  1,  8,  6,  1,  9,  8,  8,  7,  6,  -1, -1, -1, -1 },
	{ 10, 7,  6,  10, 1,  7,  1,  3,  7,  -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 7,  6,  1,  7,  10, 1,  8,  7,  1,  0,  8,  -1, -1, -1, -1 },
	{ 0,  3,  7,  0,  7,  10, 0,  10, 9,  6,  10, 7,  -1, -1, -1, -1 },
	{ 7,  6,  10, 7,  10, 8,  8,  10, 9,  -1, -1, -1, -1, -1, -1, -1 },
	{ 6,  8,  4,  11, 8,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  6,  11, 3,  0,  6,  0,  4,  6,  -1, -1, -1, -1, -1, -1, -1 },
	{ 8,  6,  11, 8,  4,  6,  9,  0,  1,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  4,  6,  9,  6,  3,  9,  3,  1,  11, 3,  6,  -1, -1, -1, -1 },
	{ 6,  8,  4,  6,  11, 8,  2,  10, 1,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  10, 3,  0,  11, 0,  6,  11, 0,  4,  6,  -1, -1, -1, -1 },
	{ 4,  11, 8,  4,  6,  11, 0,  2,  9,  2,  10, 9,  -1, -1, -1, -1 },
	{ 10, 9,  3,  10, 3,  2,  9,  4,  3,  11, 3,  6,  4,  6,  3,  -1 },
	{ 8,  2,  3,  8,  4,  2,  4,  6,  2,  -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  4,  2,  4,  6,  2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  9,  0,  2,  3,  4,  2,  4,  6,  4,  3,  8,  -1, -1, -1, -1 },
	{ 1,  9,  4,  1,  4,  2,  2,  4,  6,  -1, -1, -1, -1, -1, -1, -1 },
	{ 8,  1,  3,  8,  6,  1,  8,  4,  6,  6,  10, 1,  -1, -1, -1, -1 },
	{ 10, 1,  0,  10, 0,  6,  6,  0,  4,  -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  6,  3,  4,  3,  8,  6,  10, 3,  0,  3,  9,  10, 9,  3,  -1 },
	{ 10, 9,  4,  6,  10, 4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  9,  5,  7,  6,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  3,  4,  9,  5,  11, 7,  6,  -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  0,  1,  5,  4,  0,  7,  6,  11, -1, -1, -1, -1, -1, -1, -1 },
	{ 11, 7,  6,  8,  3,  4,  3,  5,  4,  3,  1,  5,  -1, -1, -1, -1 },
	{ 9,  5,  4,  10, 1,  2,  7,  6,  11, -1, -1, -1, -1, -1, -1, -1 },
	{ 6,  11, 7,  1,  2,  10, 0,  8,  3,  4,  9,  5,  -1, -1, -1, -1 },
	{ 7,  6,  11, 5,  4,  10, 4,  2,  10, 4,  0,  2,  -1, -1, -1, -1 },
	{ 3,  4,  8,  3,  5,  4,  3,  2,  5,  10, 5,  2,  11, 7,  6,  -1 },
	{ 7,  2,  3,  7,  6,  2,  5,  4,  9,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  5,  4,  0,  8,  6,  0,  6,  2,  6,  8,  7,  -1, -1, -1, -1 },
	{ 3,  6,  2,  3,  7,  6,  1,  5,  0,  5,  4,  0,  -1, -1, -1, -1 },
	{ 6,  2,  8,  6,  8,  7,  2,  1,  8,  4,  8,  5,  1,  5,  8,  -1 },
	{ 9,  5,  4,  10, 1,  6,  1,  7,  6,  1,  3,  7,  -1, -1, -1, -1 },
	{ 1,  6,  10, 1,  7,  6,  1,  0,  7,  8,  7,  0,  9,  5,  4,  -1 },
	{ 4,  0,  10, 4,  10, 5,  0,  3,  10, 6,  10, 7,  3,  7,  10, -1 },
	{ 7,  6,  10, 7,  10, 8,  5,  4,  10, 4,  8,  10, -1, -1, -1, -1 },
	{ 6,  9,  5,  6,  11, 9,  11, 8,  9,  -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  6,  11, 0,  6,  3,  0,  5,  6,  0,  9,  5,  -1, -1, -1, -1 },
	{ 0,  11, 8,  0,  5,  11, 0,  1,  5,  5,  6,  11, -1, -1, -1, -1 },
	{ 6,  11, 3,  6,  3,  5,  5,  3,  1,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  10, 9,  5,  11, 9,  11, 8,  11, 5,  6,  -1, -1, -1, -1 },
	{ 0,  11, 3,  0,  6,  11, 0,  9,  6,  5,  6,  9,  1,  2,  10, -1 },
	{ 11, 8,  5,  11, 5,  6,  8,  0,  5,  10, 5,  2,  0,  2,  5,  -1 },
	{ 6,  11, 3,  6,  3,  5,  2,  10, 3,  10, 5,  3,  -1, -1, -1, -1 },
	{ 5,  8,  9,  5,  2,  8,  5,  6,  2,  3,  8,  2,  -1, -1, -1, -1 },
	{ 9,  5,  6,  9,  6,  0,  0,  6,  2,  -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  5,  8,  1,  8,  0,  5,  6,  8,  3,  8,  2,  6,  2,  8,  -1 },
	{ 1,  5,  6,  2,  1,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  3,  6,  1,  6,  10, 3,  8,  6,  5,  6,  9,  8,  9,  6,  -1 },
	{ 10, 1,  0,  10, 0,  6,  9,  5,  0,  5,  6,  0,  -1, -1, -1, -1 },
	{ 0,  3,  8,  5,  6,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 5,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 11, 5,  10, 7,  5,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 11, 5,  10, 11, 7,  5,  8,  3,  0,  -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  11, 7,  5,  10, 11, 1,  9,  0,  -1, -1, -1, -1, -1, -1, -1 },
	{ 10, 7,  5,  10, 11, 7,  9,  8,  1,  8,  3,  1,  -1, -1, -1, -1 },
	{ 11, 1,  2,  11, 7,  1,  7,  5,  1,  -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  3,  1,  2,  7,  1,  7,  5,  7,  2,  11, -1, -1, -1, -1 },
	{ 9,  7,  5,  9,  2,  7,  9,  0,  2,  2,  11, 7,  -1, -1, -1, -1 },
	{ 7,  5,  2,  7,  2,  11, 5,  9,  2,  3,  2,  8,  9,  8,  2,  -1 },
	{ 2,  5,  10, 2,  3,  5,  3,  7,  5,  -1, -1, -1, -1, -1, -1, -1 },
	{ 8,  2,  0,  8,  5,  2,  8,  7,  5,  10, 2,  5,  -1, -1, -1, -1 },
	{ 9,  0,  1,  5,  10, 3,  5,  3,  7,  3,  10, 2,  -1, -1, -1, -1 },
	{ 9,  8,  2,  9,  2,  1,  8,  7,  2,  10, 2,  5,  7,  5,  2,  -1 },
	{ 1,  3,  5,  3,  7,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  7,  0,  7,  1,  1,  7,  5,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  0,  3,  9,  3,  5,  5,  3,  7,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  8,  7,  5,  9,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  8,  4,  5,  10, 8,  10, 11, 8,  -1, -1, -1, -1, -1, -1, -1 },
	{ 5,  0,  4,  5,  11, 0,  5,  10, 11, 11, 3,  0,  -1, -1, -1, -1 },
	{ 0,  1,  9,  8,  4,  10, 8,  10, 11, 10, 4,  5,  -1, -1, -1, -1 },
	{ 10, 11, 4,  10, 4,  5,  11, 3,  4,  9,  4,  1,  3,  1,  4,  -1 },
	{ 2,  5,  1,  2,  8,  5,  2,  11, 8,  4,  5,  8,  -1, -1, -1, -1 },
	{ 0,  4,  11, 0,  11, 3,  4,  5,  11, 2,  11, 1,  5,  1,  11, -1 },
	{ 0,  2,  5,  0,  5,  9,  2,  11, 5,  4,  5,  8,  11, 8,  5,  -1 },
	{ 9,  4,  5,  2,  11, 3,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  5,  10, 3,  5,  2,  3,  4,  5,  3,  8,  4,  -1, -1, -1, -1 },
	{ 5,  10, 2,  5,  2,  4,  4,  2,  0,  -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  10, 2,  3,  5,  10, 3,  8,  5,  4,  5,  8,  0,  1,  9,  -1 },
	{ 5,  10, 2,  5,  2,  4,  1,  9,  2,  9,  4,  2,  -1, -1, -1, -1 },
	{ 8,  4,  5,  8,  5,  3,  3,  5,  1,  -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  4,  5,  1,  0,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 8,  4,  5,  8,  5,  3,  9,  0,  5,  0,  3,  5,  -1, -1, -1, -1 },
	{ 9,  4,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  11, 7,  4,  9,  11, 9,  10, 11, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  8,  3,  4,  9,  7,  9,  11, 7,  9,  10, 11, -1, -1, -1, -1 },
	{ 1,  10, 11, 1,  11, 4,  1,  4,  0,  7,  4,  11, -1, -1, -1, -1 },
	{ 3,  1,  4,  3,  4,  8,  1,  10, 4,  7,  4,  11, 10, 11, 4,  -1 },
	{ 4,  11, 7,  9,  11, 4,  9,  2,  11, 9,  1,  2,  -1, -1, -1, -1 },
	{ 9,  7,  4,  9,  11, 7,  9,  1,  11, 2,  11, 1,  0,  8,  3,  -1 },
	{ 11, 7,  4,  11, 4,  2,  2,  4,  0,  -1, -1, -1, -1, -1, -1, -1 },
	{ 11, 7,  4,  11, 4,  2,  8,  3,  4,  3,  2,  4,  -1, -1, -1, -1 },
	{ 2,  9,  10, 2,  7,  9,  2,  3,  7,  7,  4,  9,  -1, -1, -1, -1 },
	{ 9,  10, 7,  9,  7,  4,  10, 2,  7,  8,  7,  0,  2,  0,  7,  -1 },
	{ 3,  7,  10, 3,  10, 2,  7,  4,  10, 1,  10, 0,  4,  0,  10, -1 },
	{ 1,  10, 2,  8,  7,  4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  9,  1,  4,  1,  7,  7,  1,  3,  -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  9,  1,  4,  1,  7,  0,  8,  1,  8,  7,  1,  -1, -1, -1, -1 },
	{ 4,  0,  3,  7,  4,  3,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 4,  8,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  10, 8,  10, 11, 8,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  0,  9,  3,  9,  11, 11, 9,  10, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  1,  10, 0,  10, 8,  8,  10, 11, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  1,  10, 11, 3,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  2,  11, 1,  11, 9,  9,  11, 8,  -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  0,  9,  3,  9,  11, 1,  2,  9,  2,  11, 9,  -1, -1, -1, -1 },
	{ 0,  2,  11, 8,  0,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 3,  2,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  3,  8,  2,  8,  10, 10, 8,  9,  -1, -1, -1, -1, -1, -1, -1 },
	{ 9,  10, 2,  0,  9,  2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 2,  3,  8,  2,  8,  10, 0,  1,  8,  1,  10, 8,  -1, -1, -1, -1 },
	{ 1,  10, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 1,  3,  8,  9,  1,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  9,  1,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ 0,  3,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }
};

static inline Vec3 interpolate(float val0, float val1, const Vec3& p0, const Vec3& p1)
{
	if (std::abs(val0) < FLT_EPSILON)
		return p0;
	if (std::abs(val1) < FLT_EPSILON)
		return p1;
	if (std::abs(val0 - val1) < FLT_EPSILON)
		return p1;
	float alpha = (0 - val0) / (val1 - val0);
	Vec3 p;
	p.x = p0.x + alpha * (p1.x - p0.x);
	p.y = p0.y + alpha * (p1.y - p0.y);
	p.z = p0.z + alpha * (p1.z - p0.z);
	return p;
}

static inline void DisplayDensity(IMergedMeshesManager* manager, AABB _bbox, uint8 dim)
{
	const AABB& bbox = _bbox;
	const Vec3 extents = (bbox.max - bbox.min);
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags old_flags = pAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags new_flags = old_flags;
	new_flags.SetCullMode(e_CullModeNone);
	new_flags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxGeom->SetRenderFlags(new_flags);

	Vec3 vertlist[12], v[8], e[2];
	float d[8][MMRM_MAX_SURFACE_TYPES];
	ISurfaceType* surfaceTypes[8][MMRM_MAX_SURFACE_TYPES];
	const float rcp = 1.f / (float)(dim - 1);
	size_t c[8];

	ColorF col0, col1, col2;
	col0.r = 0.8f;
	col0.g = 0.1f;
	col0.b = 0.2f;
	col0.a = 0.85f;

	col1.r = 0.1f;
	col1.g = 0.1f;
	col1.b = 0.6f;
	col1.a = 0.80f;

	col1.r = 0.1f;
	col1.g = 0.8f;
	col1.b = 0.1f;
	col1.a = 0.90f;

	// Extract a surface from each grid cell if the cell straddles
	// the boundary of the cell
	for (uint8 i = 0; i < dim; ++i)
		for (uint8 j = 0; j < dim; ++j)
			for (uint8 k = 0; k < dim; ++k)
			{
				const float min_x = bbox.min.x + ((i + 0) * rcp) * extents.x;
				const float min_y = bbox.min.y + ((j + 0) * rcp) * extents.y;
				const float min_z = bbox.min.z + ((k + 0) * rcp) * extents.z;

				const float max_x = bbox.min.x + ((i + 1) * rcp) * extents.x;
				const float max_y = bbox.min.y + ((j + 1) * rcp) * extents.y;
				const float max_z = bbox.min.z + ((k + 1) * rcp) * extents.z;

				memset(surfaceTypes, 0, sizeof(surfaceTypes));
				memset(d, 0, sizeof(d));

				v[0] = Vec3(min_x, min_y, min_z);
				v[1] = Vec3(max_x, min_y, min_z);
				v[2] = Vec3(min_x, max_y, min_z);
				v[3] = Vec3(max_x, max_y, min_z);
				v[4] = Vec3(min_x, min_y, max_z);
				v[5] = Vec3(max_x, min_y, max_z);
				v[6] = Vec3(min_x, max_y, max_z);
				v[7] = Vec3(max_x, max_y, max_z);

				c[0] = manager->QueryDensity(v[0], surfaceTypes[0], d[0]);
				c[1] = manager->QueryDensity(v[1], surfaceTypes[1], d[1]);
				c[2] = manager->QueryDensity(v[2], surfaceTypes[2], d[2]);
				c[3] = manager->QueryDensity(v[3], surfaceTypes[3], d[3]);
				c[4] = manager->QueryDensity(v[4], surfaceTypes[0], d[4]);
				c[5] = manager->QueryDensity(v[5], surfaceTypes[1], d[5]);
				c[6] = manager->QueryDensity(v[6], surfaceTypes[2], d[6]);
				c[7] = manager->QueryDensity(v[7], surfaceTypes[3], d[7]);

				for (size_t s = 0; s < MMRM_MAX_SURFACE_TYPES; ++s)
				{
					int32 inside = 0;
					inside |= (1 << 0) & - (d[0][s] > 0.f);
					inside |= (1 << 1) & - (d[1][s] > 0.f);
					inside |= (1 << 2) & - (d[2][s] > 0.f);
					inside |= (1 << 3) & - (d[3][s] > 0.f);
					inside |= (1 << 4) & - (d[4][s] > 0.f);
					inside |= (1 << 5) & - (d[5][s] > 0.f);
					inside |= (1 << 6) & - (d[6][s] > 0.f);
					inside |= (1 << 7) & - (d[7][s] > 0.f);
					if (inside == 0x0 || inside == 0xff)
						continue;

					int32 cubeindex = 0;
					cubeindex |= 1 & - iszero(inside & (1 << (0 + 0 * 2 + 0 * 4)));
					cubeindex |= 2 & - iszero(inside & (1 << (1 + 0 * 2 + 0 * 4)));
					cubeindex |= 4 & - iszero(inside & (1 << (1 + 1 * 2 + 0 * 4)));
					cubeindex |= 8 & - iszero(inside & (1 << (0 + 1 * 2 + 0 * 4)));
					cubeindex |= 16 & - iszero(inside & (1 << (0 + 0 * 2 + 1 * 4)));
					cubeindex |= 32 & - iszero(inside & (1 << (1 + 0 * 2 + 1 * 4)));
					cubeindex |= 64 & - iszero(inside & (1 << (1 + 1 * 2 + 1 * 4)));
					cubeindex |= 128 & - iszero(inside & (1 << (0 + 1 * 2 + 1 * 4)));

					e[0].x = min_x;
					e[0].y = min_y;
					e[0].z = min_z;
					e[1].x = max_x;
					e[1].y = max_y;
					e[1].z = max_z;

					// Define vertices on the edges intersecting the surface
					const uint16 edge_code = s_marching_cubes_edge_table[cubeindex];
					if (edge_code & 1) vertlist[0] = interpolate(d[(0 + 0 * 2 + 0 * 4)][s], d[(1 + 0 * 2 + 0 * 4)][s], Vec3(e[0].x, e[0].y, e[0].z), Vec3(e[1].x, e[0].y, e[0].z));
					if (edge_code & 2) vertlist[1] = interpolate(d[(1 + 0 * 2 + 0 * 4)][s], d[(1 + 1 * 2 + 0 * 4)][s], Vec3(e[1].x, e[0].y, e[0].z), Vec3(e[1].x, e[1].y, e[0].z));
					if (edge_code & 4) vertlist[2] = interpolate(d[(1 + 1 * 2 + 0 * 4)][s], d[(0 + 1 * 2 + 0 * 4)][s], Vec3(e[1].x, e[1].y, e[0].z), Vec3(e[0].x, e[1].y, e[0].z));
					if (edge_code & 8) vertlist[3] = interpolate(d[(0 + 1 * 2 + 0 * 4)][s], d[(0 + 0 * 2 + 0 * 4)][s], Vec3(e[0].x, e[1].y, e[0].z), Vec3(e[0].x, e[0].y, e[0].z));
					if (edge_code & 16) vertlist[4] = interpolate(d[(0 + 0 * 2 + 1 * 4)][s], d[(1 + 0 * 2 + 1 * 4)][s], Vec3(e[0].x, e[0].y, e[1].z), Vec3(e[1].x, e[0].y, e[1].z));
					if (edge_code & 32) vertlist[5] = interpolate(d[(1 + 0 * 2 + 1 * 4)][s], d[(1 + 1 * 2 + 1 * 4)][s], Vec3(e[1].x, e[0].y, e[1].z), Vec3(e[1].x, e[1].y, e[1].z));
					if (edge_code & 64) vertlist[6] = interpolate(d[(1 + 1 * 2 + 1 * 4)][s], d[(0 + 1 * 2 + 1 * 4)][s], Vec3(e[1].x, e[1].y, e[1].z), Vec3(e[0].x, e[1].y, e[1].z));
					if (edge_code & 128) vertlist[7] = interpolate(d[(0 + 1 * 2 + 1 * 4)][s], d[(0 + 0 * 2 + 1 * 4)][s], Vec3(e[0].x, e[1].y, e[1].z), Vec3(e[0].x, e[0].y, e[1].z));
					if (edge_code & 256) vertlist[8] = interpolate(d[(0 + 0 * 2 + 0 * 4)][s], d[(0 + 0 * 2 + 1 * 4)][s], Vec3(e[0].x, e[0].y, e[0].z), Vec3(e[0].x, e[0].y, e[1].z));
					if (edge_code & 512) vertlist[9] = interpolate(d[(1 + 0 * 2 + 0 * 4)][s], d[(1 + 0 * 2 + 1 * 4)][s], Vec3(e[1].x, e[0].y, e[0].z), Vec3(e[1].x, e[0].y, e[1].z));
					if (edge_code & 1024) vertlist[10] = interpolate(d[(1 + 1 * 2 + 0 * 4)][s], d[(1 + 1 * 2 + 1 * 4)][s], Vec3(e[1].x, e[1].y, e[0].z), Vec3(e[1].x, e[1].y, e[1].z));
					if (edge_code & 2048) vertlist[11] = interpolate(d[(0 + 1 * 2 + 0 * 4)][s], d[(0 + 1 * 2 + 1 * 4)][s], Vec3(e[0].x, e[1].y, e[0].z), Vec3(e[0].x, e[1].y, e[1].z));

					// Extract triangles
					for (size_t l = 0; s_marching_cubes_tri_table[cubeindex][l] != -1; l += 3)
					{
						pAuxGeom->DrawTriangle(
						  vertlist[s_marching_cubes_tri_table[cubeindex][l + 0]], col0,
						  vertlist[s_marching_cubes_tri_table[cubeindex][l + 1]], col1,
						  vertlist[s_marching_cubes_tri_table[cubeindex][l + 2]], col2);
					}
				}
			}
	pAuxGeom->SetRenderFlags(old_flags);
}

static inline void DisplayDensitySpheres(IMergedMeshesManager* manager, AABB _bbox, int dim)
{
	const AABB& bbox = _bbox;
	const Vec3 extents = (bbox.max - bbox.min);
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags old_flags = pAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags new_flags = old_flags;
	new_flags.SetCullMode(e_CullModeNone);
	new_flags.SetAlphaBlendMode(e_AlphaBlended);
	pAuxGeom->SetRenderFlags(new_flags);

	Vec3 vertlist[12], v[8], e[2];
	float d[8][MMRM_MAX_SURFACE_TYPES];
	ISurfaceType* surfaceTypes[8][MMRM_MAX_SURFACE_TYPES];
	const float rcp = 1.f / (float)(dim - 1);
	size_t c[8];

	ColorF col0, col1, col2;
	col0.r = 0.8f;
	col0.g = 0.1f;
	col0.b = 0.2f;
	col0.a = 0.85f;

	col1.r = 0.1f;
	col1.g = 0.1f;
	col1.b = 0.6f;
	col1.a = 0.80f;

	col1.r = 0.1f;
	col1.g = 0.8f;
	col1.b = 0.1f;
	col1.a = 0.90f;

	// Extract a surface from each grid cell if the cell straddles
	// the boundary of the cell
	for (int i = 0; i < dim; ++i)
		for (int j = 0; j < dim; ++j)
			for (int k = 0; k < dim; ++k)
			{
				const float min_x = bbox.min.x + ((i + 0) * rcp) * extents.x + extents.x * rcp * 0.5f;
				const float min_y = bbox.min.y + ((j + 0) * rcp) * extents.y + extents.y * rcp * 0.5f;
				const float min_z = bbox.min.z + ((k + 0) * rcp) * extents.z + extents.z * rcp * 0.5f;
				c[0] = manager->QueryDensity(Vec3(min_x, min_y, min_z), surfaceTypes[0], d[0]);

				for (size_t s = 0; s < MMRM_MAX_SURFACE_TYPES; ++s)
				{
					if (s < c[0] && d[0][s] > 0.f)
					{
						pAuxGeom->DrawSphere(Vec3(min_x, min_y, min_z), extents.x * rcp * 0.5f, Col_Red, true);
					}
					else
					{
						//pAuxGeom->DrawSphere(Vec3(min_x, min_y, min_z), extents.x*rcp*0.05f, Col_Green, false);
					}
				}
			}
	pAuxGeom->SetRenderFlags(old_flags);
}

}

#include <CryCore/TypeInfo_impl.h>

STRUCT_INFO_BEGIN(SMergedMeshInstanceCompressed)
STRUCT_VAR_INFO(pos_x, TYPE_INFO(uint32))
STRUCT_VAR_INFO(pos_y, TYPE_INFO(uint32))
STRUCT_VAR_INFO(pos_z, TYPE_INFO(uint32))
STRUCT_VAR_INFO(scale, TYPE_INFO(uint8))
STRUCT_VAR_INFO(rot, TYPE_ARRAY(4, TYPE_INFO(int8)))
STRUCT_INFO_END(SMergedMeshInstanceCompressed)

STRUCT_INFO_BEGIN(SMergedMeshSectorChunk)
STRUCT_VAR_INFO(ver, TYPE_INFO(uint32))
STRUCT_VAR_INFO(i, TYPE_INFO(uint32))
STRUCT_VAR_INFO(j, TYPE_INFO(uint32))
STRUCT_VAR_INFO(k, TYPE_INFO(uint32))
STRUCT_VAR_INFO(m_StatInstGroupID, TYPE_INFO(uint32))
STRUCT_VAR_INFO(m_nSamples, TYPE_INFO(uint32))
STRUCT_INFO_END(SMergedMeshSectorChunk)

DECLARE_JOB(
  "PVRNCullSamples"
  , TPVRNCullRenderMesh
  , SMMRMGroupHeader::CullInstances);

DECLARE_JOB(
  "PVRNUpdateSpines"
  , TPVRNUpdateRenderMeshSpines
  , SMMRMUpdateContext::MergeInstanceMeshesSpines);

DECLARE_JOB(
  "PVRNUpdateDeform"
  , TPVRNUpdateRenderMeshDeform
  , SMMRMUpdateContext::MergeInstanceMeshesDeform);

DECLARE_JOB(
  "MMRM_SortActiveInstances"
  , TMMRM_SortActiveInstances
  , CMergedMeshesManager::SortActiveInstances_Async);

DECLARE_JOB(
  "MergedMesh_InitializeSamples"
  , TMergedMesh_InitializeSamples
  , CMergedMeshRenderNode::InitializeSamples);

DECLARE_JOB(
  "MergedMesh_InitializeSpines"
  , TMergedMesh_InitializeSpines
  , CMergedMeshRenderNode::InitializeSpines);

// ToDo: pack into struct to ensure better locality in data segment
struct SMergedMeshGlobals
{
	CRY_ALIGN(16) CCamera camera;
	Vec3   lastFramesCamPos;
	uint32 frameId;
	float  dt;
	float  dtscale;
	float  abstime;
};
static CRY_ALIGN(128) SMergedMeshGlobals s_mmrm_globals;

template<typename T>
static inline void resize_list(T*& list, size_t nsize, size_t align)
{
	MEMORY_SCOPE_CHECK_HEAP();
	list = (T*) CryModuleReallocAlign(list, (nsize * sizeof(T) + (align - 1)) & ~(align - 1), align);
}

template<typename T>
static inline void pool_resize_list(T*& list, size_t nsize, size_t align)
{
	MEMORY_SCOPE_CHECK_HEAP();
	AUTO_LOCK(s_MergedMeshPoolLock);

	if (nsize == 0)
	{
		if (s_MergedMeshPool && !gEnv->IsEditor() && s_MergedMeshPool->UsableSize(list))
		{
			s_MergedMeshPool->Free(list);
			list = NULL;
			return;
		}
		CryModuleMemalignFree(list);
		list = NULL;
		return;
	}

	if (s_MergedMeshPool && !gEnv->IsEditor())
	{
		size_t osize = 0;
		if (!list || (osize = s_MergedMeshPool->UsableSize(list)) != 0u)
		{
			T* nList = (T*) s_MergedMeshPool->ReallocAlign(list, (nsize * sizeof(T) + (align - 1)) & ~(align - 1), align, "mergedmesh");
			if (nList == NULL)
			{
				memcpy(nList = (T*)CryModuleMemalign((nsize * sizeof(T) + (align - 1)) & ~(align - 1), align), list, osize);
				if (osize && list)
					s_MergedMeshPool->Free(list);
				else if (list)
					CryModuleMemalignFree(list);
			}
			if ((list = nList) != NULL)
				return;
		}
	}
	resize_list(list, nsize, align);
	return;
}

static inline void pool_free(void* ptr)
{
	AUTO_LOCK(s_MergedMeshPoolLock);
	if (s_MergedMeshPool && s_MergedMeshPool->UsableSize(ptr))
	{
		s_MergedMeshPool->Free(ptr);
		return;
	}
	CryModuleMemalignFree(ptr);
}

static void BuildSphereSet(
  AABB& bbox
  , primitives::sphere* pColliders
  , int& nColliders
  , const Vec3& pos
  , const quaternionf& q
  , const AABB& visibleAABB)
{
	Vec3 centre = ((bbox.max + bbox.min) * 0.5f);
	Vec3 size = (bbox.max - bbox.min) * 0.5f;

	int max_axis = size[0] > size[1] ? 0 : 1;
	max_axis = size[max_axis] > size[2] ? max_axis : 2;
	int min_axis =
	  size[inc_mod3[max_axis]] < size[dec_mod3[max_axis]]
	  ? inc_mod3[max_axis]
	  : dec_mod3[max_axis];

	if (size[min_axis] / (max(size[max_axis], FLT_EPSILON)) > 0.75f)
	{
		if (nColliders < MMRM_MAX_COLLIDERS)
		{
			float radius = size[min_axis];
			centre = pos + q * centre;
			if (Overlap::Sphere_AABB(Sphere(centre, radius), visibleAABB))
			{
				pColliders[nColliders].center = centre;
				pColliders[nColliders++].r = radius;
			}
		}
	}
	else
	{
		int plane_axis[2] = { max_axis, ((inc_mod3[max_axis] != min_axis) ? inc_mod3[max_axis] : dec_mod3[max_axis]) };

		Vec3 base;
		base[min_axis] = centre[min_axis];
		base[plane_axis[0]] = centre[plane_axis[0]] - size[plane_axis[0]] + size[plane_axis[0]] * 0.5f;
		base[plane_axis[1]] = centre[plane_axis[1]] - size[plane_axis[1]] + size[plane_axis[1]] * 0.5f;
		;
		float radius = size[min_axis] * 1.5f;

		Vec3 sphcent = pos + q * centre;
		if (Overlap::Sphere_AABB(Sphere(sphcent, radius), visibleAABB) && nColliders < MMRM_MAX_COLLIDERS)
		{
			pColliders[nColliders].center = sphcent;
			pColliders[nColliders++].r = radius;
		}
		for (float i = 0; i < 2.f; i += 1.f)
			for (float j = 0; j < 2.f && nColliders < MMRM_MAX_COLLIDERS; j += 1.f)
			{
				sphcent[min_axis] = base[min_axis];
				sphcent[plane_axis[0]] = base[plane_axis[0]] + ((float)i * 0.5f) * size[plane_axis[0]] * 2.f;
				sphcent[plane_axis[1]] = base[plane_axis[1]] + ((float)j * 0.5f) * size[plane_axis[1]] * 2.f;

				sphcent = pos + q * sphcent;
				if (Overlap::Sphere_AABB(Sphere(sphcent, radius), visibleAABB))
				{
					pColliders[nColliders].center = sphcent;
					pColliders[nColliders++].r = radius;
				}
			}
	}
}

static inline void ExtractSphereSet(
  IPhysicalEntity* pent
  , primitives::sphere* pColliders
  , int& nColliders
  , const AABB& visibleAABB
  , bool partsOnly = false)
{
	primitives::cylinder cylinder;
	pe_status_pos statusPos;
	pe_status_nparts statusNParts;
	int nParts = 0;
	Vec3 delta, pos;
	quaternionf q;
	if (pent->GetStatus(&statusPos) == 0)
		return;
	pos = statusPos.pos;
	q = statusPos.q;
	float scale = statusPos.scale;
	switch (pent->GetType())
	{
	case PE_LIVING:
		if ((delta = (statusPos.BBox[1] - statusPos.BBox[0])).len2() < sqr(3.5f))
		{
			nParts = pent->GetStatus(&statusNParts);
			for (statusPos.ipart = 0; statusPos.ipart < nParts && nColliders < MMRM_MAX_COLLIDERS; ++statusPos.ipart)
			{
				if (!pent->GetStatus(&statusPos))
					continue;
				if (!statusPos.pGeom)
					continue;
				switch (statusPos.pGeom->GetType())
				{
				case GEOM_CAPSULE:
				case GEOM_CYLINDER:
					statusPos.pGeom->GetPrimitive(0, &cylinder);
					for (int i = 0; i < 2 && nColliders < MMRM_MAX_COLLIDERS; ++i)
					{
						pColliders[nColliders].center = statusPos.pos + statusPos.q * (cylinder.center + ((float)(i - 1) * cylinder.axis * cylinder.hh * 2.75f));
						pColliders[nColliders++].r = cylinder.r * statusPos.scale * scale * 1.75f;
					}
					break;
				case GEOM_SPHERE:
					statusPos.pGeom->GetPrimitive(0, &pColliders[nColliders++]);
					break;
				default:
					break;
				}
			}
		}
		else
		{
			if (pent->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY)
			{
				IEntity* pEntity = (IEntity*)pent->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
				IF (!pEntity, 0)
					return;
				ICharacterInstance* pCharInstance = pEntity->GetCharacter(0);
				IF (!pCharInstance, 0)
					return;
				ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
				IF (!pSkeletonPose, 0)
					return;
				IPhysicalEntity* pEnt = pSkeletonPose->GetCharacterPhysics();
				IF (!pEnt || pEnt->GetType() != PE_ARTICULATED, 0)
					return;
				ExtractSphereSet(pEnt, pColliders, nColliders, visibleAABB, true);
			}
		}
		break;
	case PE_ARTICULATED:
		partsOnly = true;
	default:   // fallthrough Z
		if ((delta = (statusPos.BBox[1] - statusPos.BBox[0])).len2() > sqr(3.5f))
		{
			primitives::box box;
			bool added = false;
			AABB bbox = AABB(AABB::RESET);
			nParts = pent->GetStatus(&statusNParts);
			for (statusPos.ipart = 0; statusPos.ipart < nParts; ++statusPos.ipart)
			{
				statusPos.flags = status_local;
				if (!pent->GetStatus(&statusPos))
					continue;
				if (!statusPos.pGeom || (statusPos.flagsAND & geom_squashy))
					continue;
				statusPos.pGeom->GetBBox(&box);

				Vec3 centre = box.center * statusPos.scale;
				Vec3 size = box.size * statusPos.scale;

				centre = statusPos.pos + statusPos.q * centre;
				Matrix33 orientationTM = Matrix33(statusPos.q) * box.Basis.GetTransposed();

				if (partsOnly)
				{
					AABB lbbox = AABB(AABB::RESET);
					lbbox.Add(centre + orientationTM * Vec3(size.x, size.y, size.z));
					lbbox.Add(centre + orientationTM * Vec3(size.x, size.y, -size.z));
					lbbox.Add(centre + orientationTM * Vec3(size.x, -size.y, size.z));
					lbbox.Add(centre + orientationTM * Vec3(size.x, -size.y, -size.z));
					lbbox.Add(centre + orientationTM * Vec3(-size.x, size.y, size.z));
					lbbox.Add(centre + orientationTM * Vec3(-size.x, size.y, -size.z));
					lbbox.Add(centre + orientationTM * Vec3(-size.x, -size.y, size.z));
					lbbox.Add(centre + orientationTM * Vec3(-size.x, -size.y, -size.z));
					BuildSphereSet(lbbox, pColliders, nColliders, pos, q, visibleAABB);
					continue;
				}
				else
				{
					bbox.Add(centre + orientationTM * Vec3(size.x, size.y, size.z));
					bbox.Add(centre + orientationTM * Vec3(size.x, size.y, -size.z));
					bbox.Add(centre + orientationTM * Vec3(size.x, -size.y, size.z));
					bbox.Add(centre + orientationTM * Vec3(size.x, -size.y, -size.z));
					bbox.Add(centre + orientationTM * Vec3(-size.x, size.y, size.z));
					bbox.Add(centre + orientationTM * Vec3(-size.x, size.y, -size.z));
					bbox.Add(centre + orientationTM * Vec3(-size.x, -size.y, size.z));
					bbox.Add(centre + orientationTM * Vec3(-size.x, -size.y, -size.z));
					added = true;
				}
			}
			if (added)
			{
				BuildSphereSet(bbox, pColliders, nColliders, pos, q, visibleAABB);
			}
		}
		else
		{
			pColliders[nColliders].center = pos + ((statusPos.BBox[0] + statusPos.BBox[1]) * 0.5f);
			pColliders[nColliders++].r = (statusPos.BBox[0] - statusPos.BBox[1]).len() * 0.25f * scale;
		}
		break;
	}
}

static inline bool SortCollider(const primitives::sphere& a, const primitives::sphere& b)
{
	return a.r > b.r;
}

static inline void QueryColliders(primitives::sphere*& pColliders, int& nColliders, const AABB& visibleAABB)
{
	if (!pColliders) resize_list(pColliders, MMRM_MAX_COLLIDERS, 16);
	nColliders = 0;
	IPhysicalEntity* pents[MMRM_MAX_COLLIDERS];
	IPhysicalEntity** pentList = &pents[0];
	int nents = gEnv->pPhysicalWorld->GetEntitiesInBox(
	  visibleAABB.min, visibleAABB.max,
	  pentList,
	  ent_sleeping_rigid | ent_rigid | ent_living | ent_allocate_list,
	  MMRM_MAX_COLLIDERS);
	nColliders = 0;
	for (int j = 0; j < min(nents, MMRM_MAX_COLLIDERS) && nColliders < MMRM_MAX_COLLIDERS; ++j)
	{
		if (!pentList[j])
			continue;
		ExtractSphereSet(pentList[j], pColliders, nColliders, visibleAABB);
	}
	std::sort(pColliders, pColliders + nColliders, SortCollider);
	if (pents != pentList)
		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(pentList);
}

inline void QueryProjectiles(SMMRMProjectile*& pColliders, int& nColliders, const AABB& visibleAABB)
{
	if (!pColliders) resize_list(pColliders, MMRM_MAX_PROJECTILES, 16);
	nColliders = 0;
	ReadLock lock(Cry3DEngineBase::m_pMergedMeshesManager->m_ProjectileLock);
	CMergedMeshesManager* pMM = Cry3DEngineBase::m_pMergedMeshesManager;
	for (size_t i = 0, end = pMM->m_Projectiles.size(); nColliders < MMRM_MAX_PROJECTILES && i < end; ++i)
	{
		const SProjectile& projectile = pMM->m_Projectiles[i];
		if (Overlap::Lineseg_AABB(Lineseg(projectile.initial_pos, projectile.current_pos), visibleAABB) == false)
			continue;
		pColliders[nColliders].pos[0] = projectile.initial_pos;
		pColliders[nColliders].pos[1] = projectile.current_pos;
		pColliders[nColliders].dir = projectile.direction;
		pColliders[nColliders++].r = projectile.size;
	}
}

static inline void SampleWind(Vec3*& wind, const AABB& bbox)
{
	if (!wind) resize_list(wind, cube(MMRM_WIND_DIM), 16);
	for (size_t x = 0; x < MMRM_WIND_DIM; ++x)
		for (size_t y = 0; y < MMRM_WIND_DIM; ++y)
			for (size_t z = 0; z < MMRM_WIND_DIM; ++z)
			{
				wind[x + y * MMRM_WIND_DIM + z * MMRM_WIND_DIM * MMRM_WIND_DIM].x = bbox.min.x + (x / (MMRM_WIND_DIM - 1.f)) * ((bbox.max.x - bbox.min.x));
				wind[x + y * MMRM_WIND_DIM + z * MMRM_WIND_DIM * MMRM_WIND_DIM].y = bbox.min.y + (y / (MMRM_WIND_DIM - 1.f)) * ((bbox.max.y - bbox.min.y));
				wind[x + y * MMRM_WIND_DIM + z * MMRM_WIND_DIM * MMRM_WIND_DIM].z = bbox.min.z + (z / (MMRM_WIND_DIM - 1.f)) * ((bbox.max.z - bbox.min.z));
			}
#if MMRM_VISUALIZE_WINDSAMPLES
	Vec3* copy = new Vec3[cube(MMRM_WIND_DIM)];
	memcpy(copy, wind, sizeof(Vec3) * cube(MMRM_WIND_DIM));
#endif
	if (!Cry3DEngineBase::Get3DEngine()->SampleWind(wind, (MMRM_WIND_DIM)*(MMRM_WIND_DIM)*(MMRM_WIND_DIM), bbox, false))
	{
		for (size_t x = 0; x < MMRM_WIND_DIM; ++x)
			for (size_t y = 0; y < MMRM_WIND_DIM; ++y)
				for (size_t z = 0; z < MMRM_WIND_DIM; ++z)
				{
					wind[x + y * MMRM_WIND_DIM + z * MMRM_WIND_DIM * MMRM_WIND_DIM] = Vec3(0, 0, 0);
				}
	}
#if MMRM_VISUALIZE_WINDSAMPLES
	for (size_t x = 0; x < MMRM_WIND_DIM; ++x)
		for (size_t y = 0; y < MMRM_WIND_DIM; ++y)
			for (size_t z = 0; z < MMRM_WIND_DIM; ++z)
			{
				Vec3 start = copy[x + y * MMRM_WIND_DIM + z * sqr(MMRM_WIND_DIM)];
				Vec3 end = start + wind[x + y * MMRM_WIND_DIM + z * MMRM_WIND_DIM * MMRM_WIND_DIM];
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(start, Col_Black, end, Col_White);
			}
	delete[] copy;
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(bbox, false, Col_Red, eBBD_Faceted);
#endif
}

struct RenderMeshMeta
{
	IMaterial* material;
	uint32     vertices;
	uint32     indices;
};

////////////////////////////////////////////////////////////////////////////////
// Local geometry manager that contains the preprocessed geometry for the job
class CGeometryManager : public Cry3DEngineBase
{
	typedef size_t                              str_hash_t;
	typedef std::map<str_hash_t, SMMRMGeometry> GeometryMapT;

	// The geometry map of the geometry manager
	GeometryMapT m_geomMap;

	// The size in bytes of all preproccessed meshes
	size_t m_PreprocessedSize;

	// Bakes the lod into a special structure for efficient runtime merging
	bool PrepareLOD(SMMRMGeometry* geometry, CStatObj* host, size_t nLod, bool& rResubmit);

	// If statobj has "mergedmesh_autodeform" property set, extract deform info
	// to perform quick&dirty deform sim
	bool ExtractDeformLOD(SMMRMGeometry* geometry, CStatObj* host, size_t nLod);

public:
	CGeometryManager();
	~CGeometryManager();

	void Initialize(int nThreads);
	void Shutdown();

	SMMRMGeometry* GetGeometry(uint32);
	SMMRMGeometry* GetGeometry(CStatObj*);

	void ReleaseGeometry(SMMRMGeometry*);

	// Returns the main memory size of the geometry stored in the geometry manager
	size_t Size() const { return m_PreprocessedSize; }

	// Important Note: This method should not be called from any client code,
	// it is called implicitly from GetGeometry. Sadly, it has to be public
	// because DECLARE_CLASS_JOB macros only work on public methods.
	void                 PrepareGeometry(SMMRMGeometry*);

	void                 GetUsedMeshes(DynArray<string>& names);

	bool                 SyncPreparationStep();

	ILINE StatInstGroup& GetStatObjGroup(const SMMRMGeometry* geometry) const
	{
		return GetObjManager()->m_lstStaticTypes[geometry->srcGroupId];
	}
	ILINE CStatObj* GetStatObj(const SMMRMGeometry* geometry) const
	{
		return GetObjManager()->m_lstStaticTypes[geometry->srcGroupId].GetStatObj();
	}
};

DECLARE_JOB(
  "PVRNPrepGeom"
  , TPVRNPrepGeomJob
  , CGeometryManager::PrepareGeometry);

static size_t s_jobQueueIndex[2] = { 0, 0 };

CGeometryManager::CGeometryManager() : m_PreprocessedSize() {}
CGeometryManager::~CGeometryManager() {}

void CGeometryManager::Shutdown()
{
	m_geomMap.clear();
	m_PreprocessedSize = 0;
}

void CGeometryManager::Initialize(int nWorkers)
{
}

bool CGeometryManager::ExtractDeformLOD(SMMRMGeometry* geometry, CStatObj* host, size_t nLod)
{
	MEMORY_SCOPE_CHECK_HEAP();
	strided_pointer<Vec3> vtx;
	strided_pointer<ColorB> colour;
	vtx_idx* indices;
	size_t constraints_alloc = 0, nconstraints = 0,
	       nvertices = 0, nvertices_alloc = 0;
	vtx_idx* mapping = 0;
	std::set<uint32> edges, tris;
	SMMRMDeform* deform = new SMMRMDeform();
	Vec3* vertices = NULL;
	float* mass = NULL;
	SMMRMDeformConstraint* constraints = NULL;
	const int endian = 0;
	resize_list(constraints, constraints_alloc = geometry->numVtx, 16);
	resize_list(vertices, nvertices_alloc = geometry->numVtx, 16);
	resize_list(mass, nvertices_alloc, 16);
	resize_list(mapping, geometry->numVtx, 16);

	// Map incoming mesh vertices to collapsed simulation vertices
	for (size_t i = 0; i < geometry->numChunks[nLod]; ++i)
	{
		SMMRMChunk& chunk = geometry->pChunks[nLod][i];
		vtx = strided_pointer<Vec3>(&chunk.general[0].xyz, sizeof(chunk.general[0]));
		colour = strided_pointer<ColorB>(reinterpret_cast<ColorB*>(&chunk.general[0].color), sizeof(chunk.general[0]));
		indices = chunk.indices;
		for (size_t k = 0; k < chunk.nindices; k++)
		{
			const Vec3& pos = vtx[indices[k]];
			const uint8 c[2] = { colour[indices[k]].g, colour[indices[k]].b };
			const float m = (float)c[endian] / 255.f;
			size_t v_idx = (size_t)-1;
			for (size_t n = 0; n < nvertices; ++n)
			{
				if (fabs(pos.x - vertices[n].x) < FLT_EPSILON &&
				    fabs(pos.y - vertices[n].y) < FLT_EPSILON &&
				    fabs(pos.z - vertices[n].z) < FLT_EPSILON &&
				    fabsf(mass[n] - m) < FLT_EPSILON)
				{
					v_idx = n;
					break;
				}
			}
			if (v_idx == (size_t)-1)
			{
				v_idx = nvertices;
				vertices[nvertices] = pos;
				mass[nvertices++] = m;
			}
			mapping[indices[k]] = v_idx;
		}
	}
	bool collapse_ring = true;
	// Construct constraint based topology
	for (size_t i = 0; i < geometry->numChunks[nLod]; ++i)
	{
		SMMRMChunk& chunk = geometry->pChunks[nLod][i];
		indices = chunk.indices;
		// Extract bending constraints
		for (size_t j = 0; j < chunk.nvertices; ++j)
		{
			if (mass[mapping[j]] == 0.f)
				continue;
			size_t buddy[2] = { (size_t)-1, (size_t)-1 };
			std::vector<size_t> ring;
			for (size_t k = 0; k < chunk.nindices; k += 3)
			{
				size_t centre = (size_t)-1;
				for (size_t l = 0; centre == (size_t)-1 && l < 3; ++l)
					if (mapping[indices[k + l]] == mapping[j])
						centre = l;
				if (centre == (size_t)-1)
					continue;
				buddy[0] = mapping[indices[k + dec_mod3[centre]]];
				buddy[1] = mapping[indices[k + inc_mod3[centre]]];
				for (size_t l = 0; l < 2; ++l)
				{
					std::vector<size_t>::iterator where = std::lower_bound(ring.begin(), ring.end(), buddy[l]);
					if (where != ring.end() && *where == buddy[l])
						continue;
					ring.insert(where, buddy[l]);
				}
			}
			buddy[0] = (size_t)-1;
			buddy[1] = (size_t)-1;
			if (collapse_ring)
			{
				for (size_t k = 0; k < ring.size(); ++k)
				{
					Vec3 d[2];
					d[0] = (vertices[ring[k]] - vertices[mapping[j]]).GetNormalized();
					float cbest = 0.f;
					for (size_t l = 0; l < ring.size(); ++l)
					{
						if (l == k)
							continue;
						d[1] = (vertices[ring[l]] - vertices[mapping[j]]).GetNormalized();
						float angle = d[1] | d[0];
						if (angle < cbest)
						{
							cbest = angle;
							buddy[0] = ring[k];
							buddy[1] = ring[l];
						}
					}
				}
				if (buddy[0] != (size_t)-1 && buddy[1] != (size_t)-1)
				{
					for (size_t o = 0; o < nconstraints; ++o)
					{
						if (constraints[o].type == SMMRMDeformConstraint::DC_BENDING
						    && constraints[o].bending[0] == buddy[0]
						    && constraints[o].bending[1] == mapping[j]
						    && constraints[o].bending[2] == buddy[1])
							continue;
					}
					if (mass[buddy[0]] + mass[mapping[j]] + mass[buddy[1]] == 0.f)
						continue;
					if (nconstraints + 1 > constraints_alloc)
						resize_list(constraints, constraints_alloc += 0xff, 16);
					Vec3 tri[3] =
					{
						vertices[buddy[0]],
						vertices[mapping[j]],
						vertices[buddy[1]]
					};
					constraints[nconstraints].bending[0] = buddy[0];
					constraints[nconstraints].bending[1] = mapping[j];
					constraints[nconstraints].bending[2] = buddy[1];
					constraints[nconstraints].displacement = (tri[1] - (1.f / 3.f) * (tri[0] + tri[1] + tri[2])).len();
					constraints[nconstraints].k = (1.f - mass[mapping[j]]);
					constraints[nconstraints++].type = SMMRMDeformConstraint::DC_BENDING;
				}
			}
			else
			{
				for (size_t k = 0; k < ring.size(); ++k)
				{
					for (size_t l = 0; l < ring.size(); ++l)
					{
						if (l == k)
							continue;
						for (size_t o = 0; o < nconstraints; ++o)
						{
							if (constraints[o].type == SMMRMDeformConstraint::DC_BENDING
							    && constraints[o].bending[0] == ring[k]
							    && constraints[o].bending[1] == mapping[j]
							    && constraints[o].bending[2] == ring[l])
								continue;
						}
						if (mass[ring[k]] + mass[mapping[j]] + mass[ring[1]] == 0.f)
							continue;
						if (nconstraints + 1 > constraints_alloc)
							resize_list(constraints, constraints_alloc += 0xff, 16);
						Vec3 tri[3] =
						{
							vertices[ring[k]],
							vertices[mapping[j]],
							vertices[ring[l]]
						};
						constraints[nconstraints].bending[0] = (vtx_idx)ring[k];
						constraints[nconstraints].bending[1] = (vtx_idx)mapping[j];
						constraints[nconstraints].bending[2] = (vtx_idx)ring[l];
						constraints[nconstraints].displacement = (tri[1] - (1.f / 3.f) * (tri[0] + tri[1] + tri[2])).len();
						constraints[nconstraints].k = (1.f - mass[mapping[j]]);
						constraints[nconstraints++].type = SMMRMDeformConstraint::DC_BENDING;
					}
				}
			}
		}
		// Extract edge length constraints
		for (size_t j = 0; j < chunk.nindices; j += 3)
			for (size_t k = 0; k < 3; ++k)
			{
				uint32 index[2] = { mapping[indices[j + k]], mapping[indices[j + inc_mod3[k]]] };
				uint32 hash = min(index[0], index[1]) + max(index[0], index[1]) * nvertices;
				std::set<uint32>::iterator where = edges.lower_bound(hash);
				if (where != edges.end() && *where == hash)
					continue;
				if (mass[index[0]] + mass[index[1]] == 0.f)
					continue;
				edges.insert(where, hash);
				if (nconstraints + 1 > constraints_alloc)
					resize_list(constraints, constraints_alloc += 0xff, 16);
				constraints[nconstraints].edge[0] = index[0];
				constraints[nconstraints].edge[1] = index[1];
				constraints[nconstraints].edge_distance = (vertices[index[0]] - vertices[index[1]]).len();
				constraints[nconstraints].k = 1.f;
				constraints[nconstraints++].type = SMMRMDeformConstraint::DC_EDGE;
			}
	}
	// Truncate the lists to final size
	resize_list(constraints, nconstraints, 16);
	resize_list(vertices, nvertices, 16);
	resize_list(mass, nvertices, 16);
	deform->invmass = mass;
	deform->initial = vertices;
	deform->mapping = mapping;
	deform->constraints = constraints;
	deform->nconstraints = nconstraints;
	deform->nvertices = nvertices;
	geometry->deform = deform;

	// Recalculate approximate bounding volume
	geometry->aabb.Reset();
	for (size_t i = 0; i < nvertices; ++i)
	{
		if (mass[i] > 0.f)
		{
			float distance = FLT_MAX, d;
			Vec3 centre;
			for (size_t j = 0; j < nvertices; ++j)
				if (mass[j] == 0.f && (d = (vertices[i] - vertices[j]).len()) < distance)
				{
					distance = d;
					centre = vertices[j];
				}
			geometry->aabb.Add(centre + Vec3(distance, distance, distance));
			geometry->aabb.Add(centre - Vec3(distance, distance, distance));
		}
	}

	return true;
}

bool CGeometryManager::PrepareLOD(SMMRMGeometry* geometry, CStatObj* host, size_t nLod, bool& rResubmit)
{
	MEMORY_SCOPE_CHECK_HEAP();
	IRenderMesh* renderMesh = NULL;
	strided_pointer<Vec2> uv;
	strided_pointer<SPipTangents> tangs;
	strided_pointer<Vec3> vtx;
	strided_pointer<Vec3f16> norm;
	strided_pointer<uint32> colour;
	SPipQTangents* qtangents = NULL;
	SMeshBoneMapping_uint8* weights;
	bool weightsAllocated = false;
	vtx_idx* indices;
	CStatObj* statObj = (CStatObj*)host->GetLodObject(nLod, false);
	if (!statObj)
		return true;

	// ChrisR: The below block is disabled for now as it requires all levels to be re-exported
	// It will be enabled in two weeks and the above comment removed
	IF ((!(renderMesh = statObj->GetRenderMesh())) || (!gEnv->IsEditor() && (Cry3DEngineBase::m_p3DEngine->m_bInShutDown || Cry3DEngineBase::m_p3DEngine->m_bInUnload)), 0)
	{
		geometry->state = SMMRMGeometry::NO_RENDERMESH;
		return false;
	}

	renderMesh->LockForThreadAccess();
	int nvertices = renderMesh->GetVerticesCount();
	InputLayoutHandle vtx_fmt = renderMesh->GetVertexFormat();

	vtx.data = (Vec3*)renderMesh->GetPosPtr(vtx.iStride, FSL_READ);
	colour.data = (uint32*)renderMesh->GetColorPtr(colour.iStride, FSL_READ);
	uv.data = (Vec2*)renderMesh->GetUVPtr(uv.iStride, FSL_READ);
	tangs.data = (SPipTangents*)renderMesh->GetTangentPtr(tangs.iStride, FSL_READ);
	norm.data = (Vec3f16*)renderMesh->GetNormPtr(norm.iStride, FSL_READ);
	indices = renderMesh->GetIndexPtr(FSL_READ);
	weights = statObj->m_pBoneMapping;

	if (Cry3DEngineBase::GetCVars()->e_MergedMeshesTesselationSupport == 0)
	{
		norm.data = NULL;
		norm.iStride = 0;
	}
	else
		norm.data = (Vec3f16*)renderMesh->GetNormPtr(norm.iStride, FSL_READ);

	if (tangs)
	{
		resize_list(qtangents, renderMesh->GetVerticesCount(), 16);
		MeshTangentsFrameToQTangents(
		  tangs.data, tangs.iStride, nvertices,
		  qtangents, sizeof(qtangents[0]));
	}

	TRenderChunkArray& renderChunks = renderMesh->GetChunks();
	for (int i = 0; i < renderChunks.size(); ++i)
	{
		const CRenderChunk renderChunk = renderChunks[i];
		SMMRMChunk* chunk = NULL;
		if (renderChunks[i].m_nMatFlags & (MTL_FLAG_NODRAW | MTL_FLAG_COLLISION_PROXY | MTL_FLAG_RAYCAST_PROXY))
			continue;
		for (size_t j = 0; j < geometry->numChunks[nLod]; ++j)
		{
			if (geometry->pChunks[nLod][j].matId == renderChunk.m_nMatID)
			{
				chunk = &geometry->pChunks[nLod][j];
				break;
			}
		}
		if (!chunk)
		{
			resize_list(geometry->pChunks[nLod], (geometry->numChunks[nLod] + 1) * sizeof(SMMRMChunk), 16);
			chunk = new(&geometry->pChunks[nLod][geometry->numChunks[nLod]++])SMMRMChunk(renderChunk.m_nMatID);
		}
		chunk->nvertices_alloc += renderChunk.nNumVerts;
		chunk->nindices_alloc += renderChunk.nNumIndices;
	}
	// Patch envelope based weighting into geometry if RC skipped weighting step for lower lods.
	if (weights == NULL && host->m_pBoneMapping)
	{
		int nvertices = renderMesh->GetVerticesCount();
		weights = new SMeshBoneMapping_uint8[nvertices];

		weightsAllocated = true;
		for (int i = 0; i < nvertices; ++i)
		{
			float dist = FLT_MAX;
			size_t idx = (size_t)-1;
			for (int j = 0, boneIdx = 1; j < host->m_nSpines; boneIdx += host->m_pSpines[j++].nVtx - 1)
			{
				for (int k = 0; k < host->m_pSpines[j].nVtx - 1; ++k)
				{
					float t = 0.f, dp = Distance::Point_Lineseg(
					  vtx[i], Lineseg(host->m_pSpines[j].pVtx[k], host->m_pSpines[j].pVtx[k + 1]), t);
					if (dp < dist)
					{
						dist = dp;
						idx = boneIdx + k;
					}
				}
			}

			weights[i].boneIds[0] = static_cast<uint8>(idx);
			weights[i].weights[0] = 255;
			for (int j = 1; j < 4; ++j)
			{
				weights[i].boneIds[j] = 0;
				weights[i].weights[j] = 0;
			}
		}
	}
	for (size_t i = 0; i < geometry->numChunks[nLod]; ++i)
	{
		SMMRMChunk* chunk = &geometry->pChunks[nLod][i];
		resize_list(chunk->general, chunk->nvertices_alloc, 16);
		resize_list(chunk->indices, chunk->nindices_alloc, 16);
		if (qtangents)  resize_list(chunk->qtangents, chunk->nvertices_alloc, 16);
		if (norm) resize_list(chunk->normals, chunk->nvertices_alloc, 16);
		if (weights) resize_list(chunk->weights, chunk->nvertices_alloc, 16);
	}
	for (int i = 0; i < renderChunks.size(); ++i)
	{
		const CRenderChunk renderChunk = renderChunks[i];
		SMMRMChunk* chunk = NULL;
		if (renderChunks[i].m_nMatFlags & (MTL_FLAG_NODRAW | MTL_FLAG_COLLISION_PROXY | MTL_FLAG_RAYCAST_PROXY))
			continue;
		for (size_t j = 0; j < geometry->numChunks[nLod]; ++j)
		{
			if (geometry->pChunks[nLod][j].matId == renderChunk.m_nMatID)
			{
				chunk = &geometry->pChunks[nLod][j];
				break;
			}
		}
		assert(chunk);
		PREFAST_ASSUME(chunk);
		for (size_t j = renderChunk.nFirstIndexId; j < renderChunk.nFirstIndexId + renderChunk.nNumIndices; ++j)
		{
			vtx_idx index = indices[j];
			vtx_idx chkidx = (vtx_idx) - 1;
			SMMRMBoneMapping weightsForIndex;
			if (weights) weightsForIndex = weights[index];
			for (size_t k = 0; k < chunk->nvertices; ++k)
			{
				IF (vtx[index] != chunk->general[k].xyz, 1) continue;
				IF (colour && colour[index] != chunk->general[k].color.dcolor, 1) continue;
				IF (uv && uv[index] != chunk->general[k].st, 1) continue;
				IF (qtangents && qtangents[index] != chunk->qtangents[k], 1) continue;
				IF (norm && norm[index].ToVec3() != chunk->normals[k], 1) continue;
				IF (weights && chunk->weights[k] != weightsForIndex, 1) continue;
				chkidx = (vtx_idx)k;
				break;
			}
			if (chkidx == (vtx_idx) - 1)
			{
				assert(chunk->nvertices < chunk->nvertices_alloc);
				size_t usedSpines = 0;
				chunk->general[chunk->nvertices].xyz = vtx[index];
				if (colour) chunk->general[chunk->nvertices].color.dcolor = colour[index];
				if (uv) chunk->general[chunk->nvertices].st = uv[index];
				if (qtangents) chunk->qtangents[chunk->nvertices] = qtangents[index];
				if (weights) chunk->weights[chunk->nvertices] = weightsForIndex;
				if (norm) chunk->normals[chunk->nvertices] = norm[index].ToVec3();
				for (size_t k = 0; weights && k < 4; ++k)
					if (weights[index].weights[k]) ++usedSpines;
				geometry->maxSpinesPerVtx = max(geometry->maxSpinesPerVtx, usedSpines);
				chkidx = (vtx_idx)(chunk->nvertices++);
			}
			assert(chunk->nindices < chunk->nindices_alloc);
			chunk->indices[chunk->nindices++] = chkidx;
		}
	}
	for (size_t i = 0; i < geometry->numChunks[nLod]; ++i)
	{
		SMMRMChunk* chunk = &geometry->pChunks[nLod][i];
		resize_list(chunk->general, chunk->nvertices, 16);
		resize_list(chunk->indices, chunk->nindices, 16);
		if (qtangents) resize_list(chunk->qtangents, chunk->nvertices, 16);
		if (weights) resize_list(chunk->weights, chunk->nvertices, 16);
		if (norm) resize_list(chunk->normals, chunk->nvertices, 16);
		if (nLod == 0)
		{
			geometry->numVtx += (chunk->nvertices_alloc = chunk->nvertices);
			geometry->numIdx += (chunk->nindices_alloc = chunk->nindices);
		}
		resize_list(chunk->skin_vertices, nvertices, SMMRMSkinVertex_ALIGN);
		for (size_t j = 0; j < chunk->nvertices; ++j)
		{
			memset(&chunk->skin_vertices[j], 0, sizeof(chunk->skin_vertices[j]));
			chunk->skin_vertices[j].pos = chunk->general[j].xyz;
			chunk->skin_vertices[j].uv = chunk->general[j].st;
			chunk->skin_vertices[j].colour = chunk->general[j].color;
			if (chunk->normals)
				chunk->skin_vertices[j].normal = chunk->normals[j];
			if (chunk->weights)
			{
				chunk->skin_vertices[j].SetWeights(chunk->weights[j].weights);
				chunk->skin_vertices[j].SetBoneIds(chunk->weights[j].boneIds);
			}
			if (chunk->qtangents)
				chunk->skin_vertices[j].qt = chunk->qtangents[j];
		}
	}

	if (qtangents) CryModuleMemalignFree(qtangents);
	renderMesh->UnlockStream(VSF_GENERAL);
	renderMesh->UnlockStream(VSF_TANGENTS);
	renderMesh->UnlockStream(VSF_QTANGENTS);
	renderMesh->UnlockIndexStream();
	renderMesh->UnLockForThreadAccess();
	if (weightsAllocated) delete[] weights;
	MEMORY_CHECK_HEAP();
	return true;
}

void CGeometryManager::PrepareGeometry(SMMRMGeometry* geometry)
{
	if (!geometry)
	{
		return;
	}
	CStatObj* statObj = NULL;
	if (geometry->is_obj)
		statObj = geometry->srcObj;
	else
	{
		statObj = GetStatObj(geometry);
	}
	if (!statObj)
	{
		geometry->state = SMMRMGeometry::NO_STATINSTGROUP;
		return;
	}
	bool success = true;
	float maxSpineLen = 0, len = 0;
	int i = 0, j = 0;
	size_t resultingSize = 0;
	for (i = 0; success && i < (int)MAX_STATOBJ_LODS_NUM; ++i)
	{
		bool bResubmit = false;
		success &= PrepareLOD(geometry, statObj, i, bResubmit);
		IF (bResubmit, 0)
		{
			CrySleep(10);
			TPVRNPrepGeomJob job(geometry);
			job.SetClassInstance(this);
			job.RegisterJobState(&geometry->geomPrepareState);
			job.SetBlocking();
			job.SetPriorityLevel(JobManager::eLowPriority);
			job.Run();
			return;
		}
	}
	if (!!CryStringUtils::stristr(statObj->m_szProperties, "mergedmesh_deform"))
	{
		success &= ExtractDeformLOD(geometry, statObj, 0);
	}
	else if (statObj->m_nSpines && statObj->m_pSpines)
	{
		resize_list(geometry->pSpineInfo, geometry->numSpines = statObj->m_nSpines, 16);
		for (i = 0; i < statObj->m_nSpines; ++i)
		{
			const SSpine& spine = statObj->m_pSpines[i];
			size_t baseVtx = geometry->numSpineVtx;
			resize_list(geometry->pSpineVtx, geometry->numSpineVtx += (geometry->pSpineInfo[i].nSpineVtx = spine.nVtx), 16);
			for (j = 0; j < spine.nVtx; ++j)
				geometry->pSpineVtx[baseVtx + j].pt = spine.pVtx[j];
			for (j = 0, maxSpineLen = 0; j < spine.nVtx; ++j)
			{
				int j1 = baseVtx + j;
				int j2 = baseVtx + j - (iszero(j - (spine.nVtx - 1)) - 1);
				maxSpineLen += (len = (geometry->pSpineVtx[j2].pt - geometry->pSpineVtx[j1].pt).len());
				geometry->pSpineVtx[j1].h = 0.f;
				if (j == 0 && spine.nVtx > 2) // special handling for the first segment
				{
					Vec3 tri[3];
					tri[0] = geometry->pSpineVtx[baseVtx + j + 0].pt - Vec3(0, 0, 1);
					tri[1] = geometry->pSpineVtx[baseVtx + j + 0].pt;
					tri[2] = geometry->pSpineVtx[baseVtx + j + 1].pt;
					geometry->pSpineVtx[baseVtx + j].h = (tri[1] - (1.f / 3.f) * (tri[0] + tri[1] + tri[2])).len();
				}
				if (j > 0 && j < spine.nVtx - 1)
				{
					Vec3 tri[3];
					tri[0] = geometry->pSpineVtx[baseVtx + j - 1].pt;
					tri[1] = geometry->pSpineVtx[baseVtx + j].pt;
					tri[2] = geometry->pSpineVtx[baseVtx + j + 1].pt;
					geometry->pSpineVtx[j1].h = (tri[1] - (1.f / 3.f) * (tri[0] + tri[1] + tri[2])).len();
				}
				geometry->pSpineVtx[j1].len = len;
			}
			geometry->pSpineInfo[i].fSpineLen = maxSpineLen;
		}
	}
	if (success)
	{
		geometry->state = SMMRMGeometry::PREPARED;
		resultingSize = geometry->Size() + sizeof(SMMRMGeometry);
		m_PreprocessedSize += resultingSize;

		if (GetCVars()->e_DebugGeomPrep > 0)
		{
			string szGeomName = statObj->m_szFileName + "_" + statObj->m_szGeomName;
			CryLogAlways("PVRN geometry '%s' prepared, size %" PRISIZE_T " bytes"
			             , (szGeomName.c_str() ? szGeomName.c_str() : "unknown")
			             , resultingSize);
			for (i = 0; i < (int)MAX_STATOBJ_LODS_NUM; ++i)
			{
				size_t vcnt = 0u, icnt = 0u;
				if (geometry->pChunks[i])
				{
					for (j = 0; (size_t)j < geometry->numChunks[i]; ++j)
					{
						vcnt += geometry->pChunks[i][j].nvertices;
						icnt += geometry->pChunks[i][j].nindices;
					}
					CryLogAlways("\tLOD %d %8" PRISIZE_T " vertices %8" PRISIZE_T " indices"
					             , i
					             , vcnt
					             , icnt);
				}
			}
		}
	}
}

SMMRMGeometry* CGeometryManager::GetGeometry(uint32 groupId)
{
	StatInstGroup* group = &GetObjManager()->m_lstStaticTypes[groupId];
	CStatObj* statObj = group->GetStatObj();
	if (!statObj)
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "CGeometryManager::GetGeometry group has no statobj");
		statObj = m_pObjManager->m_pDefaultCGF;
	}

	string filename = (statObj->m_szFileName + "_" + statObj->m_szGeomName);
	str_hash_t fileNameHash = hasher(filename.c_str(), filename.length());

	GeometryMapT::iterator it = m_geomMap.lower_bound(fileNameHash);
	if (it == m_geomMap.end() || it->first != fileNameHash)
	{
		// keep an extra reference to the static geometry (needed because we want to extract material sometimes...)
		statObj->AddRef();

		it = m_geomMap.insert(it, std::make_pair(fileNameHash, SMMRMGeometry(groupId)));
		it->second.aabb = statObj->GetAABB();
		if (!gEnv->IsEditor())
		{
			TPVRNPrepGeomJob job(&it->second);
			job.SetClassInstance(this);
			job.RegisterJobState(&it->second.geomPrepareState);
			job.SetBlocking();
			job.SetPriorityLevel(JobManager::eLowPriority);
			job.Run();
		}
		else
		{
			PrepareGeometry(&it->second);
		}
	}
	++it->second.refCount;
	return &it->second;
}

SMMRMGeometry* CGeometryManager::GetGeometry(CStatObj* statObj)
{
	string filename = (statObj->m_szFileName + "_" + statObj->m_szGeomName);
	str_hash_t fileNameHash = hasher(filename.c_str(), filename.length());

	GeometryMapT::iterator it = m_geomMap.lower_bound(fileNameHash);
	if (it == m_geomMap.end() || it->first != fileNameHash)
	{
		it = m_geomMap.insert(it, std::make_pair(fileNameHash, SMMRMGeometry(statObj)));
		it->second.aabb = statObj->GetAABB();

		// keep an extra reference to the static geometry (needed because we want to extract material sometimes...)
		if (NULL != it->second.srcObj)
		{
			it->second.srcObj->AddRef();
		}
		if (!gEnv->IsEditor())
		{
			TPVRNPrepGeomJob job(&it->second);
			job.SetClassInstance(this);
			job.RegisterJobState(&it->second.geomPrepareState);
			job.SetBlocking();
			job.SetPriorityLevel(JobManager::eLowPriority);
			job.Run();
		}
		else
		{
			PrepareGeometry(&it->second);
		}
	}
	++it->second.refCount;
	return &it->second;
}

void CGeometryManager::ReleaseGeometry(SMMRMGeometry* geom)
{
	GeometryMapT::iterator it = m_geomMap.begin();
	while (it != m_geomMap.end())
	{
		if (&it->second == geom)
		{
			break;
		}
		++it;
	}
	if (it == m_geomMap.end())
	{
		CryLogAlways("CGeometryManager::ReleaseGeometry: geometry to release not found!");
	}
	else if (--it->second.refCount == 0)
	{
		// release reference to the static geometry
		if (it->second.is_obj)
		{
			SAFE_RELEASE(it->second.srcObj);
		}
		else
		{
			CStatObj* pStatObj = GetStatObj(&it->second);
			if (pStatObj)
			{
				pStatObj->Release();
			}
		}

		m_PreprocessedSize -= it->second.Size();
		m_PreprocessedSize -= sizeof(SMMRMGeometry);
		m_geomMap.erase(it);
	}
}

void CGeometryManager::GetUsedMeshes(DynArray<string>& names)
{
	for (GeometryMapT::const_iterator it = m_geomMap.begin(), end = m_geomMap.end();
	     it != end; ++it)
	{
		const SMMRMGeometry& geom = it->second;
		gEnv->pJobManager->WaitForJob(const_cast<JobManager::SJobState&>(geom.geomPrepareState));
		CStatObj* pObj = NULL;
		if (geom.is_obj)
			pObj = geom.srcObj;
		else if(geom.srcGroupId < (uint32)GetObjManager()->m_lstStaticTypes.Count())
		{
			pObj = GetStatObj(&geom);
		}
		if (pObj)
			names.push_back(pObj->m_szFileName);
	}
}

bool CGeometryManager::SyncPreparationStep()
{
	bool success = true;
	for (GeometryMapT::const_iterator it = m_geomMap.begin(), end = m_geomMap.end();
	     it != end; ++it)
	{
		const SMMRMGeometry& geom = it->second;
		gEnv->pJobManager->WaitForJob(const_cast<JobManager::SJobState&>(geom.geomPrepareState));
		success &= geom.state == SMMRMGeometry::PREPARED;
	}
	return success;
}

CGeometryManager s_GeomManager;

// This a definition of the callback function that is called when variable change.
static void UpdateRatios(ICVar* pVar)
{
	MEMORY_SCOPE_CHECK_HEAP();
	CMergedMeshesManager* pManager = ((C3DEngine*)(gEnv->p3DEngine))->m_pMergedMeshesManager;
	if (!pVar || !pManager)
		return;
	if (!strcmp(pVar->GetName(), "e_MergedMeshesViewDistRatio"))
	{
		pManager->UpdateViewDistRatio(pVar->GetFVal());
	}
	if (!strcmp(pVar->GetName(), "e_MergedMeshesLodRatio"))
	{
		pManager->UpdateLodRatio(pVar->GetFVal());
	}
}

SMMRMGroupHeader::~SMMRMGroupHeader()
{
	MEMORY_SCOPE_CHECK_HEAP();
	s_GeomManager.ReleaseGeometry(procGeom);
	if (instances) pool_free(instances);
	if (spines) pool_free(spines);
	if (deform_vertices) pool_free(deform_vertices);
	if (visibleChunks) pool_free(visibleChunks);
	if (proxies) pool_free(proxies);
}

CMergedMeshRenderNode::CMergedMeshRenderNode()
	: m_LastDrawFrame()
	, m_LastUpdateFrame()
	, m_DistanceSQ()
	, m_groups()
	, m_nGroups()
	, m_pos()
	, m_zRotation(0.0f)
	, m_visibleAABB(AABB::RESET)
	, m_internalAABB(AABB::RESET)
	, m_Instances()
	, m_SpineCount((size_t)-1)
	, m_DeformCount((size_t)-1)
	, m_nLod(-1)
	, m_nActive()
	, m_nVisible()
	, m_needsStaticMeshUpdate()
	, m_needsDynamicMeshUpdate()
	, m_needsPostRenderStatic()
	, m_needsPostRenderDynamic()
	, m_ownsGroups(1)
	, m_State(INITIALIZED)
	, m_RenderMode(NOT_VISIBLE)
	, m_wind()
	, m_density()
	, m_Colliders()
	, m_nColliders()
	, m_Projectiles()
	, m_nProjectiles()
	, m_cullState()
	, m_updateState()
	, m_spineInitializationState()
	, m_SizeInVRam()
	, m_renderMeshes()
	, m_pReadStream()
	, m_SpinesActive()
{
	GetInstCount(GetRenderNodeType())++;

	memset(m_surface_types, 0x0, sizeof(m_surface_types));
	SetViewDistUnlimited();
	m_fWSMaxViewDist = FLT_MAX;
	m_visibleAABB.Reset();
	SetRndFlags(ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS, true);
	if (GetCVars()->e_MergedMeshesOutdoorOnly)
		SetRndFlags(GetRndFlags() | ERF_OUTDOORONLY, true);
}
CMergedMeshRenderNode::~CMergedMeshRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	MEMORY_SCOPE_CHECK_HEAP();

	if (m_pReadStream)
	{
		m_pReadStream->Abort();
		m_pReadStream = NULL;
	}

	Clear();
	Get3DEngine()->FreeRenderNodeState(this);
	m_pMergedMeshesManager->RemoveMergedMesh(this);
}

void CMergedMeshRenderNode::Clear()
{
	MEMORY_SCOPE_CHECK_HEAP();
	for (size_t i = 0; i < m_nGroups; ++i)
		if (m_groups[i].procGeom)
			gEnv->pJobManager->WaitForJob(m_groups[i].procGeom->geomPrepareState);
	DeleteRenderMesh(RUT_STATIC);
	DeleteRenderMesh(RUT_DYNAMIC);
	m_renderMeshes[RUT_STATIC].clear();
	m_renderMeshes[RUT_DYNAMIC].clear();
	if (m_groups && m_ownsGroups)
	{
		for (size_t i = 0; i < m_nGroups; ++i)
		{
			if (gEnv->IsEditor() && m_groups[i].proxies)
			{
				for (size_t j = 0; j < m_groups[i].numSamples; ++j)
				{
					assert(m_groups[i].proxies && m_groups[i].proxies[j]);
					PREFAST_ASSUME(m_groups[i].proxies && m_groups[i].proxies[j]);
					m_groups[i].proxies[j]->m_host = NULL;
					delete m_groups[i].proxies[j];
				}
			}
			m_groups[i].~SMMRMGroupHeader();
		}
		CryModuleMemalignFree(m_groups);
	}
	m_groups = NULL;
	m_nGroups = 0u;
	if (m_wind) { CryModuleMemalignFree(m_wind); m_wind = NULL; }
	if (m_density) { CryModuleMemalignFree(m_density); m_density = NULL; }
	if (m_Colliders) { CryModuleMemalignFree(m_Colliders); m_Colliders = NULL; }
	if (m_Projectiles) { CryModuleMemalignFree(m_Projectiles); m_Projectiles = NULL; }
}

bool CMergedMeshRenderNode::PrepareRenderMesh(RENDERMESH_UPDATE_TYPE type)
{
	// Make sure that all preparation jobs have completed until then.
	MEMORY_SCOPE_CHECK_HEAP();
	std::vector<size_t> groups(m_nGroups);
	std::vector<SMMRM>& meshes = m_renderMeshes[type];
	std::iota(groups.begin(), groups.end(), 0);
	Vec3 extents = (m_visibleAABB.max - m_visibleAABB.min) * 0.5f;
	Vec3 origin = m_pos - extents;
	bool allDone = true;
	Quat q;
	for (size_t i = 0, end = groups.size(); i < end; ++i)
	{
		SMMRMGroupHeader* header = NULL;
		size_t index = groups[i];
		if (m_groups[index].procGeom->geomPrepareState.IsRunning() == true)
		{
			allDone = false;
			continue;
		}
		else
		{
			switch (m_groups[index].procGeom->state)
			{
			case SMMRMGeometry::PREPARED:
				header = &m_groups[index];
				break;
			case SMMRMGeometry::NO_RENDERMESH:
				assert(0);
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR,
				           "merged mesh '%s' had no rendermesh during merging",
				           GetStatObj(m_groups[index].instGroupId)->m_szFileName.c_str());
				break;
			case SMMRMGeometry::CREATED:
				assert(0);
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR,
				           "merged mesh '%s' has an invalid state during merging",
				           GetStatObj(m_groups[index].instGroupId)->m_szFileName.c_str());
				break;
			case SMMRMGeometry::NO_STATINSTGROUP:
				assert(0);
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR,
				           "merged mesh had no valid statinst group");
				break;
			default:
				break;
			}
		}
		if (header && header->numSamples)
		{
			const SMMRMGeometry* procGeom = header->procGeom;
			StatInstGroup& srcGroup = GetStatObjGroup(header->instGroupId);
			header->is_dynamic = procGeom->numSpines == 0u;
			if ((int)(header->is_dynamic = procGeom->numSpines > 0u) != (int)type)
				continue;
			IMaterial* material =
			  procGeom->is_obj
			  ? procGeom->srcObj->GetMaterial()
			  : (srcGroup.pMaterial ? srcGroup.pMaterial.get() : srcGroup.GetStatObj()->GetMaterial());
			SMMRM* mesh = NULL;
			for (std::size_t j = 0; j < meshes.size(); ++j)
			{
				if (meshes[j].mat == material)
				{
					mesh = &meshes[j];
					break;
				}
			}
			if (!mesh)
			{
				meshes.push_back(SMMRM());
				mesh = &meshes.back();
				mesh->mat = material;
				mesh->chunks = procGeom->numChunks[0];
			}
			resize_list(header->visibleChunks, header->numVisbleChunks = procGeom->numChunks[0], 16);
			for (size_t j = 0; j < procGeom->numChunks[0]; ++j)
			{
				const SMMRMChunk& chunk = procGeom->pChunks[0][j];
				mesh->hasTangents |= !!chunk.qtangents;
				mesh->hasNormals |= !!chunk.normals;
			}
		}
		// Editor does not stream instances, so we create spines on demand here
		if (gEnv->IsEditor() && header && header->procGeom->numSpines)
		{
			const float fExtents = c_MergedMeshesExtent;
			Vec3 vInternalAABBMin = m_internalAABB.min;
			resize_list(header->spines, header->numSamples * header->procGeom->numSpineVtx, 128);
			for (size_t j = 0, base = 0; j < header->numSamples; ++j)
			{
				uint16 pos[3] = { uint16(header->instances[j].pos_x), uint16(header->instances[j].pos_y), uint16(header->instances[j].pos_z) };
				const float fScale = (1.f / VEGETATION_CONV_FACTOR) * header->instances[j].scale;
				DecompressQuat(q, header->instances[j]);
				Matrix34 wmat = CreateRotationQ(q, ConvertInstanceAbsolute(pos, vInternalAABBMin, m_pos, m_zRotation, fExtents));
				for (size_t k = 0; k < header->procGeom->numSpineVtx; ++k, ++base)
				{
#if MMRM_SIMULATION_USES_FP32
					header->spines[base].pt = wmat * (header->procGeom->pSpineVtx[k].pt * fScale);
					header->spines[base].vel = Vec3(0, 0, 0);
#else
					header->spines[base].pt = header->procGeom->pSpineVtx[k].pt;
					header->spines[base].vel = Vec3(0, 0, 0);
#endif
				}
			}
			m_SpinesActive = true;
		}
		// Editor does not stream instances, so we create deformation on demand here
		if (gEnv->IsEditor() && header && header->procGeom->deform)
		{
			resize_list(header->deform_vertices, header->numSamples * header->procGeom->deform->nvertices, 16);
			for (size_t j = 0, base = 0; j < header->numSamples; ++j)
			{
				const float fScale = (1.f / VEGETATION_CONV_FACTOR) * header->instances[j].scale;
				DecompressQuat(q, header->instances[j]);
				Matrix34 wmat = CreateRotationQ(q, ConvertInstanceAbsolute(header->instances[j], m_internalAABB.min, m_pos, m_zRotation, c_MergedMeshesExtent)) * Matrix34::CreateScale(Vec3(fScale, fScale, fScale));
				for (size_t k = 0; k < header->procGeom->deform->nvertices; ++k, ++base)
				{
					header->deform_vertices[base].pos[0] = header->deform_vertices[base].pos[1] = wmat * header->procGeom->deform->initial[k];
					header->deform_vertices[base].vel = Vec3(0, 0, 0);
				}
			}
			m_SpinesActive = true;
		}
	}
	return allDone;
}

bool CMergedMeshRenderNode::Setup(
  const AABB& aabb
  , const AABB& visAbb
  , const PodArray<SProcVegSample>* samples)
{
	MEMORY_SCOPE_CHECK_HEAP();
	m_internalAABB = aabb;
	m_visibleAABB = visAbb;
	m_pos = (aabb.max + aabb.min) * 0.5f;
	m_initPos = m_pos;

	for (size_t i = 0; samples && i < samples->size(); ++i)
		AddInstance(samples->GetAt(i));

	m_State = DIRTY;
	return true;
}

bool CMergedMeshRenderNode::AddGroup(uint32 statInstGroupId, uint32 numSamples)
{
	MEMORY_SCOPE_CHECK_HEAP();
	if (Cry3DEngineBase::GetCVars()->e_MergedMeshes == 0)
		return false;

	StatInstGroup* group = &GetStatObjGroup(statInstGroupId);
	for (uint32 ic = 0; ic < numSamples; ic += min(numSamples - ic, MMRM_MAX_SAMPLES_PER_BATCH))
	{
		SMMRMGroupHeader* header = NULL;
		resize_list(m_groups, m_nGroups + 1, 128);
		header = new(&m_groups[m_nGroups++])SMMRMGroupHeader;

		header->procGeom = s_GeomManager.GetGeometry(statInstGroupId);
		header->instGroupId = statInstGroupId;
		header->numSamples = min(numSamples - ic, MMRM_MAX_SAMPLES_PER_BATCH);
		header->maxViewDistance = 0;
		header->lodRationNorm = 0;
		header->splitGroup = ic > 0;
		if (group) // Just in case the group is not present
		{
			header->maxViewDistance = group->fVegRadius * group->fMaxViewDistRatio * 100.f;
			header->lodRationNorm = group->fLodDistRatio;
			if (!(header->specMismatch = !CheckMinSpec(group->minConfigSpec)))
				m_Instances += min(numSamples - ic, MMRM_MAX_SAMPLES_PER_BATCH);
			header->physConfig.Update(group, 1.f);
		}
	}

	return true;
}

IRenderNode* CMergedMeshRenderNode::AddInstance(const SProcVegSample& sample)
{
	MEMORY_SCOPE_CHECK_HEAP();
	assert(gEnv->IsEditor());

	SMMRMGroupHeader* header = NULL;
	StatInstGroup* group = &GetStatObjGroup(sample.InstGroupId);
	IF (!group, 0)
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR,
		           "CMergedMeshRenderNode::AddInstance no valid statinstgroup given");
		return NULL;
	}
	CStatObj* statObj = group->GetStatObj();
	IF (!statObj, 0)
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR,
		           "CMergedMeshRenderNode::AddInstance no valid statobj given");
		return NULL;
	}
	IF (!statObj->GetRenderMesh(), 0)
	{
		static CStatObj* lastReportedObj = NULL;
		if (lastReportedObj != statObj)
			FileWarning(0, statObj->GetFilePath(), "CMergedMeshRenderNode::AddInstance: CGF file does not support mesh auto merging");
		lastReportedObj = statObj;
		return NULL;
	}

	const Vec3 extents = (m_visibleAABB.max - m_visibleAABB.min) * 0.5f;
	const Vec3 origin = m_pos - extents;
	size_t headerIndex = (size_t)-1;

	SMMRMGeometry* procGeom = s_GeomManager.GetGeometry(sample.InstGroupId);

	gEnv->pJobManager->WaitForJob(m_cullState);
	for (size_t i = 0; i < m_nGroups; ++i)
	{
		if (m_groups[i].instGroupId == sample.InstGroupId &&
		    m_groups[i].numSamples < min((size_t)MMRM_MAX_SAMPLES_PER_BATCH, 0xffffu / procGeom->numVtx))
		{
			headerIndex = i;
			header = &m_groups[i];
			break;
		}
	}
	gEnv->pJobManager->WaitForJob(m_updateState);
	if (headerIndex == (size_t)-1)
	{
		resize_list(m_groups, m_nGroups + 1, 128);
		header = new(&m_groups[headerIndex = m_nGroups++])SMMRMGroupHeader;
		header->procGeom = s_GeomManager.GetGeometry(sample.InstGroupId);
		header->instGroupId = sample.InstGroupId;
	}
	else
	{
		s_GeomManager.ReleaseGeometry(procGeom);
	}
	if (header->numSamples + 1 >= header->numSamplesAlloc)
	{
		pool_resize_list(header->instances, header->numSamplesAlloc += 0xff, 128);
		if (gEnv->IsEditor())
			resize_list(header->proxies, header->numSamplesAlloc, 16);
	}

	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;

	ConvertInstanceRelative(header->instances[header->numSamples], sample.pos, m_internalAABB.min, fExtentsRec);
	header->instances[header->numSamples].scale = sample.scale;
	CompressQuat(sample.q, header->instances[header->numSamples]);
	header->instances[header->numSamples++].lastLod = -2;

	// in case the values changed, reapply them here as otherwise the header setup will never get updated
	header->maxViewDistance = group->fVegRadius * group->fMaxViewDistRatio * 100.f;
	header->lodRationNorm = group->fLodDistRatio;

	if (header->spines)
	{
		pool_free(header->spines);
		header->spines = NULL;
	}
	if (header->deform_vertices)
	{
		pool_free(header->deform_vertices);
		header->deform_vertices = NULL;
	}

	m_visibleAABB.Add(sample.pos + header->procGeom->aabb.max);
	m_visibleAABB.Add(sample.pos + header->procGeom->aabb.min);
	Get3DEngine()->UnRegisterEntityAsJob(this);
	Get3DEngine()->RegisterEntity(this);

	assert(m_internalAABB.IsContainPoint(sample.pos));

	++m_Instances;
	m_State = DIRTY;

	if (gEnv->IsEditor())
	{
		header->proxies[header->numSamples - 1] = new CMergedMeshInstanceProxy(this, headerIndex, header->numSamples - 1);
		return header->proxies[header->numSamples - 1];
	}

	return NULL;
}

size_t CMergedMeshRenderNode::RemoveInstance(size_t headerIndex, size_t instanceIndex)
{
	MEMORY_SCOPE_CHECK_HEAP();
	assert(headerIndex < m_nGroups);

	SMMRMGroupHeader* header = &m_groups[headerIndex];
	gEnv->pJobManager->WaitForJob(m_cullState);
	gEnv->pJobManager->WaitForJob(m_updateState);

	assert(instanceIndex < header->numSamples);
	std::swap(header->instances[instanceIndex], header->instances[header->numSamples - 1]);
	if (gEnv->IsEditor() && instanceIndex != header->numSamples - 1)
	{
		std::swap(header->proxies[instanceIndex], header->proxies[header->numSamples - 1]);
		header->proxies[instanceIndex]->m_sampleIndex = instanceIndex;
	}
	--header->numSamples;
	if (header->numSamplesAlloc - header->numSamples > 0xff)
	{
		pool_resize_list(header->instances, header->numSamplesAlloc -= 0xff, 128);
		if (gEnv->IsEditor())
		{
			resize_list(header->proxies, header->numSamplesAlloc, 16);
		}
	}
	if (header->spines)
	{
		CryModuleMemalignFree(header->spines);
		header->spines = NULL;
	}
	if (header->deform_vertices)
	{
		CryModuleMemalignFree(header->deform_vertices);
		header->deform_vertices = NULL;
	}

	m_State = DIRTY;
	return --m_Instances;
}

void CMergedMeshRenderNode::QueryColliders()
{
	::QueryColliders(m_Colliders, m_nColliders, m_visibleAABB);
}

void CMergedMeshRenderNode::QueryProjectiles()
{
	::QueryProjectiles(m_Projectiles, m_nProjectiles, m_visibleAABB);
}

void CMergedMeshRenderNode::SampleWind()
{
	::SampleWind(m_wind, m_internalAABB);
}

bool CMergedMeshRenderNode::DeleteRenderMesh(RENDERMESH_UPDATE_TYPE type, bool block, bool zap)
{
	MEMORY_SCOPE_CHECK_HEAP();
	while (!SyncAllJobs())
	{
		if (!block) return false;
	}
	// Clear the rendermesh update structures
	std::vector<SMMRM>& renderMeshes = m_renderMeshes[type];
	for (size_t j = 0; j < renderMeshes.size(); ++j)
	{
		if (zap)
			stl::free_container(renderMeshes[j].updates);
		else
		{
			for (size_t i = 0; i < renderMeshes[j].updates.size(); ++i)
				renderMeshes[j].updates[i].chunks.clear();
		}
		if (zap)
			stl::free_container(renderMeshes[j].rms);
		else
			renderMeshes[j].rms.clear();
		renderMeshes[j].vertices = 0;
		renderMeshes[j].indices = 0;
	}
	m_SizeInVRam = 0u;

	if (type == RUT_STATIC)
	{
		InvalidatePermanentRenderObject();
		m_manipulationFrame = -1;
	}		

	return true;
}

void CMergedMeshRenderNode::CreateRenderMesh(RENDERMESH_UPDATE_TYPE type, const SRenderingPassInfo& passInfo)
{
	MEMORY_SCOPE_CHECK_HEAP();
	const int use_spines = GetCVars()->e_MergedMeshesUseSpines;
	std::vector<SMMRM>& renderMeshes = m_renderMeshes[type];
	const size_t num_rms = renderMeshes.size();
	m_SizeInVRam = 0;

	// Prime memory
	for (uintptr_t i = (uintptr_t)&m_groups[0]; i < (uintptr_t)&m_groups[m_nGroups]; i += 128u)
		CryPrefetch((void*)i);

	PREFAST_SUPPRESS_WARNING(6255)
	size_t * num_updates = (size_t*)alloca(sizeof(size_t) * num_rms);
	PREFAST_SUPPRESS_WARNING(6255)
	size_t * groups_mesh = (size_t*)alloca(sizeof(size_t) * m_nGroups);
	PREFAST_SUPPRESS_WARNING(6255)
	size_t * num_vi = (size_t*)alloca(sizeof(size_t) * m_nGroups);

	memset(num_updates, 0x0, sizeof(size_t) * num_rms);
	memset(groups_mesh, 0x0, sizeof(size_t) * m_nGroups);
	memset(num_vi, 0x0, sizeof(size_t) * m_nGroups);

	// For all registered groups, build the update contexts
	// and grab required vertex and index counts
	for (size_t i = 0, j = 0; i < m_nGroups; ++i)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		SMMRMGroupHeader* group = &m_groups[i];
		IF (group->numSamples == 0 || group->specMismatch || ((int)type != (int)group->is_dynamic), 0)
			continue;
		size_t ii = 0, iv = 0;
		for (j = 0; j < group->numVisbleChunks; ++j)
		{
			ii += group->visibleChunks[j].indices;
			iv += group->visibleChunks[j].vertices;
		}
		if ((num_vi[i] = ii + iv) == 0)
			continue;
		mmrm_assert(ii > 0 && iv > 0);
		StatInstGroup* instGroup = &GetStatObjGroup(group->instGroupId);
		if (gEnv->IsEditor())
			group->physConfig.Update(instGroup, 1.f);
		IMaterial* material = instGroup->pMaterial
		                      ? instGroup->pMaterial.get()
		                      : const_cast<IMaterial*>(instGroup->GetStatObj()->GetMaterial());
		SMMRM* mesh = NULL;
		for (j = 0; j < num_rms; ++j)
		{
			if (renderMeshes[j].mat == material)
			{
				mesh = &renderMeshes[j];
				break;
			}
		}
		if (mesh)
		{
			++num_updates[j];
			groups_mesh[i] = j;
		}
		else
		{
			// we didn't find a proper mesh for the given group material, most likely the node wasn't properly reset after a material change
			__debugbreak();
		}
	}

	// Pre-allocate the number of concurrent updates each mesh will receive
	for (size_t j = 0; j < num_rms; ++j)
	{
		renderMeshes[j].updates.resize(num_updates[j]);
		num_updates[j] = 0;
	}

	// For all registered groups, build the update contexts
	// and grab required vertex and index counts
	for (size_t i = 0, j = 0; i < m_nGroups; ++i)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		SMMRMGroupHeader* group = &m_groups[i];
		IF (group->numSamples == 0 || group->specMismatch || ((int)type != (int)group->is_dynamic) || num_vi[i] == 0, 0)
			continue;
		SMMRM* mesh = &renderMeshes[groups_mesh[i]];
		SMMRMUpdateContext& update = mesh->updates[num_updates[groups_mesh[i]]++];
		update.group = group;
		update.chunks.resize(group->numVisbleChunks);
		for (size_t k = 0; k < group->numVisbleChunks; ++k)
		{
			mesh->indices += (update.chunks[k].icnt = group->visibleChunks[k].indices);
			mesh->vertices += (update.chunks[k].vcnt = group->visibleChunks[k].vertices);
			update.chunks[k].matId = group->visibleChunks[k].matId;
		}
	}

#if MMRM_RENDER_DEBUG && MMRM_VISUALIZE_WINDFIELD
	if (GetCVars()->e_MergedMeshesDebug) VisualizeWindField(m_wind, m_internalAABB, true, true);
#endif

	// Loop over the rendermeshes and dispatch the update passes
	for (size_t i = 0, nrm = renderMeshes.size(); i < nrm; ++i)
	{
		MEMORY_SCOPE_CHECK_HEAP();
		SMMRM* mesh = &renderMeshes[i];
		if (mesh->updates.empty() || mesh->vertices == 0 || mesh->indices == 0)
			continue;
		size_t k = 0, vertices = 0, indices = 0;
		do
		{
			// Assign contiguous renderchunks per group subchunk, split into multiple rendermeshes
			// if the combined size of the rendermeshes exceed the allowed vertex count for 16bit
			// indices.
			std::vector<CRenderChunk> rmchunks;
			rmchunks.reserve(mesh->updates.size());
			CRenderChunk chunk;
			size_t iv = 0, ii = 0, beg = k, accum = 0;
			for (; k < mesh->updates.size(); ++k)
			{
				SMMRMUpdateContext* update = &mesh->updates[k];
				for (size_t j = 0, acumm = 0, nc = update->chunks.size(); j < nc; ++j)
					accum += update->chunks[j].vcnt;
				if (iv + accum > 0xffff)
					goto done;
				for (size_t j = 0, nc = update->chunks.size(); j < nc; ++j)
				{
					if (update->chunks[j].matId != chunk.m_nMatID)
					{
						if (chunk.nNumIndices && chunk.nNumVerts)
							rmchunks.push_back(chunk);
						chunk.m_nMatID = update->chunks[j].matId;
						chunk.nFirstIndexId = ii;
						chunk.nFirstVertId = iv;
						chunk.nNumIndices = 0;
						chunk.nNumVerts = 0;
					}
					update->chunks[j].ioff = ii;
					update->chunks[j].voff = iv;
					chunk.nNumIndices += update->chunks[j].icnt;
					ii += update->chunks[j].icnt;
					chunk.nNumVerts += update->chunks[j].vcnt;
					iv += update->chunks[j].vcnt;
				}
			}
done:
			if (chunk.nNumIndices && chunk.nNumVerts)
				rmchunks.push_back(chunk);

			mmrm_assert(iv <= 0xffff);
			// Create a new rendermesh and dispatch the asynchronous updates
			_smart_ptr<IRenderMesh> rm = GetRenderer()->CreateRenderMeshInitialized(
			  NULL, iv, EDefaultInputLayouts::P3S_C4B_T2S, NULL, ii,
			  prtTriangleList, "MergedMesh", "MergedMesh", eRMT_Dynamic);
			rm->LockForThreadAccess();
			m_SizeInVRam += (sizeof(SVF_P3S_C4B_T2S) + sizeof(SPipTangents)) * iv;
			m_SizeInVRam += sizeof(vtx_idx) * ii;

			strided_pointer<SPipTangents> tgtBuf;
			strided_pointer<Vec3f16> vtxBuf;
			strided_pointer<Vec3f16> nrmBuf;
			vtx_idx* idxBuf = NULL;

			vtxBuf.data = (Vec3f16*)rm->GetPosPtrNoCache(vtxBuf.iStride, FSL_CREATE_MODE);
			IF (mesh->hasNormals, 1)
				nrmBuf.data = (Vec3f16*)rm->GetNormPtr(nrmBuf.iStride, FSL_CREATE_MODE);
			IF (mesh->hasTangents, 1)
				tgtBuf.data = (SPipTangents*)rm->GetTangentPtr(tgtBuf.iStride, FSL_CREATE_MODE);
			idxBuf = rm->GetIndexPtr(FSL_CREATE_MODE);

			if (!rm->CanUpdate() || !vtxBuf || !idxBuf ||
			    (mesh->hasNormals && !nrmBuf) ||
			    (mesh->hasTangents && (!tgtBuf)))
			{
				rm->UnlockStream(VSF_GENERAL);
				rm->UnlockStream(VSF_TANGENTS);
				rm->UnlockIndexStream();
				rm->UnLockForThreadAccess();
				continue;
			}
			for (size_t j = beg; j < k; ++j)
			{
				SMMRMUpdateContext* update = &mesh->updates[j];
				update->general = (SVF_P3S_C4B_T2S*)vtxBuf.data;
				update->tangents = tgtBuf.data;
				update->normals = nrmBuf.data;
				update->idxBuf = idxBuf;
				update->updateFlag = rm->SetAsyncUpdateState();
				update->colliders = m_Colliders;
				update->ncolliders = m_nColliders;
				update->projectiles = m_Projectiles;
				update->nprojectiles = m_nProjectiles;
				update->max_iter = 1;
				update->dt = s_mmrm_globals.dt;
				update->abstime = s_mmrm_globals.abstime;
				update->zRotation = m_zRotation;
				update->rotationOrigin = m_pos;
				update->_min = m_internalAABB.min;
				update->_max = m_internalAABB.max;
				update->wind = m_wind;
				update->use_spines = use_spines && m_SpinesActive;
				update->frame_count = passInfo.GetMainFrameID();
#if MMRM_USE_BOUNDS_CHECK
				for (size_t u = 0; u < update->chunks.size(); ++u)
				{
					if (((SVF_P3S_C4B_T2S*)vtxBuf.data) + update->chunks[u].voff > (((SVF_P3S_C4B_T2S*)vtxBuf.data) + iv)) __debugbreak();
					if (idxBuf + update->chunks[u].ioff > idxBuf + ii) __debugbreak();
				}
				update->general_end = ((SVF_P3S_C4B_T2S*)vtxBuf.data) + iv;
				update->tangents_end = ((SPipTangents*)tgtBuf.data) + iv;
				update->idx_end = idxBuf + ii;
#endif
#if MMRM_USE_JOB_SYSTEM
				if (update->group->deform_vertices)
				{
					TPVRNUpdateRenderMeshDeform job(&s_mmrm_globals.camera, 0u);
					job.RegisterJobState(&m_updateState);
					job.SetClassInstance(update);
					job.SetPriorityLevel(JobManager::eLowPriority);
					job.Run();
				}
				else
				{
					TPVRNUpdateRenderMeshSpines job(&s_mmrm_globals.camera, 0u);
					job.RegisterJobState(&m_updateState);
					job.SetClassInstance(update);
					job.SetPriorityLevel(JobManager::eLowPriority);
					job.Run();
				}
#else
				if (update->group->deform_vertices)
					update->MergeInstanceMeshesDeform(&s_mmrm_globals.camera, 0u);
				else
					update->MergeInstanceMeshesSpines(&s_mmrm_globals.camera, 0u);
#endif
			}
			rm->SetRenderChunks(&rmchunks[0], rmchunks.size(), false);
			rm->UnLockForThreadAccess();
			mesh->rms.push_back(rm);
			vertices += iv;
			indices += ii;
		}
		while (vertices < mesh->vertices && indices < mesh->indices);
	}

	m_LastUpdateFrame = passInfo.GetMainFrameID();

	if (type == RUT_STATIC)
	{
		InvalidatePermanentRenderObject();
		m_manipulationFrame = -1;
	}	
}

void CMergedMeshRenderNode::Render(const struct SRendParams& EntDrawParams, const SRenderingPassInfo& passInfo)
{
	MEMORY_SCOPE_CHECK_HEAP();
	Vec3 extents, origin;
	uint32 frameId;

	if (!passInfo.RenderVegetation() || GetCVars()->e_MergedMeshes == 0)
		return;

	size_t i = 0;
	extents = (m_visibleAABB.max - m_visibleAABB.min) * 0.5f;
	origin = m_pos - extents;
	frameId = passInfo.GetMainFrameID();

	// ToDo : make nicer
	const CCamera& currentCam = passInfo.GetCamera();
	if (frameId != s_mmrm_globals.frameId)
	{
		s_mmrm_globals.lastFramesCamPos = s_mmrm_globals.camera.GetPosition();
		s_mmrm_globals.camera = currentCam;
		s_mmrm_globals.dt = gEnv->pTimer->GetFrameTime();
		s_mmrm_globals.dtscale = gEnv->pTimer->GetTimeScale();
		s_mmrm_globals.abstime = gEnv->pTimer->GetCurrTime();
		s_mmrm_globals.frameId = frameId;
	}

	const Vec3& camPos = currentCam.GetPosition();
	const float sqDiagonalInternal = m_internalAABB.GetRadiusSqr() * GetCVars()->e_MergedMeshesInstanceDist;
	const float sqDiagonal = m_visibleAABB.GetRadiusSqr();
	const float sqDistanceToBox = Distance::Point_AABBSq(camPos, m_visibleAABB);
	int nLod = 0;
	uint32 lodFrequency[] = { 3, 5, 7, 11 };

	if (!passInfo.IsGeneralPass())
	{
		if (sqDistanceToBox < sqDiagonalInternal)
		{
			float len = (passInfo.GetCamera().GetPosition() - m_pos).len();
			if (GroupsCastShadows(RUT_STATIC)) RenderRenderMesh(RUT_STATIC, len, passInfo);
			if (GroupsCastShadows(RUT_DYNAMIC)) RenderRenderMesh(RUT_DYNAMIC, len, passInfo);
		}
		return;
	}

	m_rendParams = EntDrawParams;
	bool switched = false;
	if ((passInfo.IsZoomInProgress() || passInfo.IsZoomActive() || sqDistanceToBox < sqDiagonalInternal) && m_SpinesActive)
	{
		switched = m_RenderMode != DYNAMIC;
		m_nLod = (int)min(max(sqDistanceToBox / (max(GetCVars()->e_MergedMeshesLodRatio * sqDiagonal, 0.001f)), 0.f), ((float)MAX_STATOBJ_LODS_NUM - 1.f));
		m_RenderMode = DYNAMIC;
	}
	else
	{
		switched = m_RenderMode != INSTANCED;
		m_nLod = (switched) ? -1 : m_nLod;
		if (m_nLod != -1)
		{
			const float lastDistanceToBox = Distance::Point_AABBSq(s_mmrm_globals.lastFramesCamPos, m_visibleAABB);
			if (abs(lastDistanceToBox - sqDistanceToBox) > sqDiagonalInternal * 0.25f)
				m_nLod = -1;
		}
		m_nColliders = 0;
		m_RenderMode = INSTANCED;
	}

	m_LastDrawFrame = frameId;
	bool requiresPostRender = false;

	switch (m_State)
	{
	case INITIALIZED:
		break;
	case DIRTY:
		DeleteRenderMesh(RUT_DYNAMIC);
		m_renderMeshes[RUT_DYNAMIC].clear();
		DeleteRenderMesh(RUT_STATIC);
		m_renderMeshes[RUT_STATIC].clear();
		m_usedMaterials.clear();
		m_nLod = -1;
		m_State = PREPARING;
	case PREPARING:
		// We can't do anything if the prepping stage is still running
		if (PrepareRenderMesh(RUT_STATIC) == false || PrepareRenderMesh(RUT_DYNAMIC) == false)
			break;
		m_State = PREPARED;
		// In Editor mode, we don't stream so prepared == streamed in
		if (gEnv->IsEditor())
			m_State = STREAMED_IN;
		else
			break;
	case STREAMED_IN:
		for (signed pass = 0; pass < 2; ++pass)
		{
			switch (m_RenderMode)
			{
			case DYNAMIC:
				if (pass == RUT_STATIC)
					goto static_fallthrough;
				if (!(s_mmrm_globals.dt > 0.f))
				{
					RenderRenderMesh(
					  RUT_DYNAMIC
					  , (passInfo.GetCamera().GetPosition() - m_pos).len()
					  , passInfo);
				}
				else
				{
					bool dispatched = false;
					DeleteRenderMesh(RUT_DYNAMIC);
					for (i = 0; i < m_nGroups; ++i)
					{
						if (m_groups[i].numSamples == 0u || m_groups[i].specMismatch || !m_groups[i].is_dynamic)
							continue;
						int flags = (int)MMRM_CULL_FRUSTUM | MMRM_CULL_DISTANCE;
#if MMRM_USE_JOB_SYSTEM
						mmrm_printf("starting culljob on %#x header (%#x instance %#x numsamples)\n",
						            (unsigned int)&m_groups[i]
						            , (unsigned int)m_groups[i].instances
						            , (unsigned int)m_groups[i].numSamples);
						TPVRNCullRenderMesh job(&s_mmrm_globals.camera, &m_internalAABB.min, &m_pos, m_zRotation, flags);
						job.RegisterJobState(&m_cullState);
						job.SetClassInstance(&m_groups[i]);
						job.SetPriorityLevel(JobManager::eHighPriority);
						job.Run();
						//gEnv->pJobManager->WaitForJob(m_cullState);
#else
						m_groups[i].CullInstances(&s_mmrm_globals.camera, &m_internalAABB.min, &m_pos, m_zRotation, flags);
#endif
						dispatched = true;
					}
					if (dispatched)
					{
						m_needsDynamicMeshUpdate = true;
						m_needsPostRenderDynamic = true;
						requiresPostRender = true;
					}
				}
				break;
static_fallthrough:
			case INSTANCED:
				nLod = (int)min(max(sqDistanceToBox / (max(GetCVars()->e_MergedMeshesLodRatio * sqDiagonal, 0.001f)), 0.f), ((float)MAX_STATOBJ_LODS_NUM - 1.f));
				if ((nLod != m_nLod || (pass == RUT_DYNAMIC && (frameId - m_LastUpdateFrame) > (uint32)(lodFrequency[nLod & 3]))) && s_mmrm_globals.dt > 0.f)
				{
					bool dispatched = false;
					DeleteRenderMesh((RENDERMESH_UPDATE_TYPE)pass);
					for (i = 0; i < m_nGroups; ++i)
					{
						if (m_groups[i].numSamples == 0u || m_groups[i].specMismatch || (pass != (signed)m_groups[i].is_dynamic))
							continue;
						int flags = (int)(nLod << MMRM_LOD_SHIFT) | MMRM_CULL_LOD | MMRM_CULL_DISTANCE;
#if MMRM_USE_JOB_SYSTEM
						TPVRNCullRenderMesh job(&s_mmrm_globals.camera, &m_internalAABB.min, &m_pos, m_zRotation, flags);
						job.RegisterJobState(&m_cullState);
						job.SetClassInstance(&m_groups[i]);
						job.SetPriorityLevel(JobManager::eHighPriority);
						job.Run();
#else
						m_groups[i].CullInstances(&s_mmrm_globals.camera, &m_internalAABB.min, &m_pos, m_zRotation, flags);
#endif
						dispatched = true;
					}
					m_nLod = nLod;
					if (dispatched)
					{
						switch (pass)
						{
						case RUT_STATIC:
							m_needsStaticMeshUpdate = true;
							m_needsPostRenderStatic = true;
							break;
						case RUT_DYNAMIC:
							m_needsDynamicMeshUpdate = true;
							m_needsPostRenderDynamic = true;
							break;
						}
						requiresPostRender = true;
					}
					break;
				}
				RenderRenderMesh(
				  (RENDERMESH_UPDATE_TYPE)pass
				  , (passInfo.GetCamera().GetPosition() - m_pos).len()
				  , passInfo);
				break;
			}
		}
		break;
	case STREAMING:
	case PREPARED:
		break;
	case RENDERNODE_STATE_ERROR:
		CryLogAlways("MergedMeshRendernode is in an Error state\n");
	default:
		//__debugbreak();
		break;
	}
	if (requiresPostRender)
		Cry3DEngineBase::m_pMergedMeshesManager->RegisterForPostRender(this);
}

bool CMergedMeshRenderNode::GroupsCastShadows(RENDERMESH_UPDATE_TYPE type)
{
	for (size_t i = 0; i < m_nGroups; ++i)
	{
		if (m_groups[i].numSamples == 0u || m_groups[i].specMismatch || (type != (signed)m_groups[i].is_dynamic))
			continue;
		StatInstGroup& srcGroup = GetObjManager()->m_lstStaticTypes[m_groups[i].instGroupId];
		if (srcGroup.nCastShadowMinSpec <= GetCVars()->e_ObjShadowCastSpec)
			return true;
	}
	return false;
}

void CMergedMeshRenderNode::RenderRenderMesh(
  RENDERMESH_UPDATE_TYPE type
  , const float distance
  , const SRenderingPassInfo& passInfo
  )
{
	const auto pTempData = m_pTempData.load();
	if (!pTempData)
	{
		CRY_ASSERT(false);
		return;
	}

	// Prepare object model matrix
	const auto objMat = Matrix34::CreateTranslationMat(m_pos);

	CRenderObject* ro = nullptr;
	if (GetObjManager()->AddOrCreatePersistentRenderObject(type == RUT_STATIC ? pTempData : nullptr, ro, nullptr, objMat, passInfo))
		return;

	SSectorTextureSet* pTerrainTexInfo = nullptr;
	byte ucSunDotTerrain = 255;
	const float fRenderQuality = min(1.f, max(1.f - distance / (GetCVars()->e_MergedMeshesActiveDist * 0.3333f), 0.f));

	bool bUseTerrainColor = false;
	for (size_t i = 0; i < m_nGroups; ++i)
	{
		StatInstGroup* pInstGroup = &GetStatObjGroup(m_groups[i].instGroupId);
		if (pInstGroup->bUseTerrainColor)
			bUseTerrainColor = true;
	}

	if (GetCVars()->e_VegetationUseTerrainColor && bUseTerrainColor)
	{
		float fRadius = GetBBox().GetRadius();
		Vec3 vTerrainNormal = GetTerrain()->GetTerrainSurfaceNormal(GetBBox().GetCenter(), fRadius);
		ucSunDotTerrain = (uint8)(CLAMP((vTerrainNormal.Dot(Get3DEngine()->GetSunDirNormalized())) * 255.f, 0, 255));
		GetObjManager()->FillTerrainTexInfo(static_cast<COctreeNode*>(m_pOcNode),
			distance,
			pTerrainTexInfo,
			static_cast<COctreeNode*>(m_pOcNode)->GetObjectsBBox());
	}

	ColorF ambientColor = m_rendParams.AmbientColor;
	ro->SetAmbientColor(ambientColor, passInfo);
	ro->m_fAlpha = m_rendParams.fAlpha;
	ro->m_ObjFlags = FOB_TRANS_MASK | FOB_INSHADOW | FOB_DYNAMIC_OBJECT;
	if (pTerrainTexInfo)
	{
		pTempData->userData.bTerrainColorWasUsed = true;
		ambientColor.a = clamp_tpl((1.0f / 255.f * ucSunDotTerrain), 0.f, 1.f);
		ro->SetAmbientColor(ambientColor, passInfo);
		ro->m_ObjFlags |= FOB_BLEND_WITH_TERRAIN_COLOR;
		ro->m_data.m_pTerrainSectorTextureInfo = pTerrainTexInfo;
		ro->m_nTextureID = -(int)pTerrainTexInfo->nSlot0 - 1; // nTextureID is set only for proper batching, actual texture id is same for all terrain sectors

		{
			float fBlendDistance = GetCVars()->e_VegetationUseTerrainColorDistance;

			if (fBlendDistance == 0)
			{
				ro->m_data.m_fMaxViewDistance = m_fWSMaxViewDist;
			}
			else if (fBlendDistance > 0)
			{
				ro->m_data.m_fMaxViewDistance = fBlendDistance;
			}
			else // if(fBlendDistance < 0)
			{
				ro->m_data.m_fMaxViewDistance = abs(fBlendDistance) * m_fWSMaxViewDist;
			}
		}
	}
	ro->m_nRenderQuality = (uint16)(fRenderQuality * 65535.0f);
	ro->m_fDistance = distance;
	if (Get3DEngine()->IsTessellationAllowed(ro, passInfo))
		ro->m_ObjFlags |= FOB_ALLOW_TESSELLATION;

	bool first = true;
	for (const auto& mesh : m_renderMeshes[type])
	{
		CRenderObject* roMat = first ? ro : GetRenderer()->EF_DuplicateRO(ro, passInfo);

		roMat->m_pCurrMaterial = mesh.mat;

		for (auto& rm : mesh.rms)
		{
			IF (!rm, 0)
				continue;

			rm->Render(roMat, passInfo);
		}

		first = false;
	}
}

bool CMergedMeshRenderNode::PostRender(const SRenderingPassInfo& passInfo)
{
	MEMORY_SCOPE_CHECK_HEAP();
	FUNCTION_PROFILER_3DENGINE;

	const uint32 frameId = passInfo.GetMainFrameID();

	// Sadly because of the interleaved renderchunks structure, we can
	// only start building a mesh when all culling tasks have completed.
	IF (m_cullState.IsRunning(), 1)
		return true;

	const float distance = (s_mmrm_globals.camera.GetPosition() - m_pos).len();

	if (m_needsStaticMeshUpdate)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "MMRM PR CR static");
		CreateRenderMesh(RUT_STATIC, passInfo);
		m_needsStaticMeshUpdate = false;
	}
	if (m_needsDynamicMeshUpdate)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "MMRM PR CR dynamic");
		CreateRenderMesh(RUT_DYNAMIC, passInfo);
		m_needsDynamicMeshUpdate = false;
	}

	// Check tempData in case object was validated before
	Get3DEngine()->CheckAndCreateRenderNodeTempData(this, passInfo);

	if (m_needsPostRenderStatic)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE,"MMRM PR RR static");
		RenderRenderMesh(RUT_STATIC, distance, passInfo);
		m_needsPostRenderStatic = false;
	}
	if (m_needsPostRenderDynamic)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE,"MMRM PR RR dynamic");
		RenderRenderMesh(RUT_DYNAMIC, distance, passInfo);
		m_needsPostRenderDynamic = false;
	}
	return false;
}

bool CMergedMeshRenderNode::Compile(byte* pData, int& nSize, string* pName, std::vector<struct IStatInstGroup*>* pVegGroupTable, const Vec3& segmentOffset)
{
	// Construct the unique id for the sector
	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;
	if (pData)
	{
		char token[256];
		int _i = (int)floorf(abs(m_pos.x + segmentOffset.x) * fExtentsRec);
		int _j = (int)floorf(abs(m_pos.y + segmentOffset.y) * fExtentsRec);
		int _k = (int)floorf(abs(m_pos.z + segmentOffset.z) * fExtentsRec);
		cry_sprintf(token, "sector_%d_%d_%d", _i, _j, _k);
		*pName = token;

		Vec3 extents = (m_visibleAABB.max - m_visibleAABB.min) * 0.5f;
		Vec3 origin = m_pos - extents;

		uint8* pPtr = pData;

		// Compile the headers and instances for this merged mesh instance
		for (size_t i = 0; i < NumGroups(); ++i)
		{
			const SMMRMGroupHeader* header = Group(i);
			StatInstGroup& group = GetStatObjGroup(header->instGroupId);
			int nItemId = CObjManager::GetItemId<IStatInstGroup>(pVegGroupTable, &group);
			if (nItemId < 0)
				assert(!"StatObj not found in StatInstGroupTable on exporting");

			SMergedMeshSectorChunk sectorChunk;
			sectorChunk.m_StatInstGroupID = nItemId;
			sectorChunk.m_nSamples = header->numSamples;
			sectorChunk.i = (uint32)floorf(abs(m_pos.x + segmentOffset.x) * fExtentsRec);
			sectorChunk.j = (uint32)floorf(abs(m_pos.y + segmentOffset.y) * fExtentsRec);
			sectorChunk.k = (uint32)floorf(abs(m_pos.z + segmentOffset.z) * fExtentsRec);
			sectorChunk.ver = c_MergedMeshChunkVersion;
			AddToPtr(pPtr, sectorChunk, GetPlatformEndian());

			for (size_t j = 0; j < header->numSamples; ++j)
			{
				SMergedMeshInstanceCompressed sampleChunk;
				sampleChunk.pos_x = header->instances[j].pos_x;
				sampleChunk.pos_y = header->instances[j].pos_y;
				sampleChunk.pos_z = header->instances[j].pos_z;
				sampleChunk.rot[0] = header->instances[j].qx;
				sampleChunk.rot[1] = header->instances[j].qy;
				sampleChunk.rot[2] = header->instances[j].qz;
				sampleChunk.rot[3] = header->instances[j].qw;
				sampleChunk.scale = header->instances[j].scale;
				AddToPtr(pPtr, sampleChunk, GetPlatformEndian());
			}
		}
	}
	else
	{
		for (size_t i = 0; i < NumGroups(); ++i)
		{
			nSize += sizeof(SMergedMeshSectorChunk);
			nSize += Group(i)->numSamples * sizeof(SMergedMeshInstanceCompressed);
		}
	}

	return true;
}

void CMergedMeshRenderNode::FillSamples(DynArray<Vec2>& samples)
{
	// Compile the headers and instances for this merged mesh instance
	const float fExtents = c_MergedMeshesExtent;
	if (m_State != STREAMED_IN)
		return;
	for (size_t i = 0; i < NumGroups(); ++i)
	{
		const SMMRMGroupHeader* header = Group(i);
		const AABB& aabb = header->procGeom->aabb;
		const Vec3& centre = aabb.GetCenter();
		const Vec3& size = aabb.GetSize();
		samples.reserve(samples.size() + header->numSamples * 4);
		for (size_t j = 0; j < header->numSamples; ++j)
		{
			Vec3 pos = ConvertInstanceAbsolute(header->instances[j], m_internalAABB.min, m_pos, m_zRotation, fExtents);
			samples.push_back(Vec2(pos.x - size.x, pos.y - size.y));
			samples.push_back(Vec2(pos.x - size.x, pos.y + size.y));
			samples.push_back(Vec2(pos.x + size.x, pos.y + size.y));
			samples.push_back(Vec2(pos.x + size.x, pos.y - size.y));
		}
	}
}

void CMergedMeshRenderNode::ActivateSpines()
{
	if (m_spineInitializationState.IsRunning() || m_updateState.IsRunning())
		return;
	if (m_State != STREAMED_IN || m_SpinesActive)
		return;
	for (size_t i = 0; i < NumGroups(); ++i)
		CryPrefetch(m_groups[i].procGeom);
	for (size_t i = 0; i < NumGroups(); ++i)
	{
		SMMRMGroupHeader* header = &m_groups[i];
		if (header->specMismatch)
			continue;
		if (header->procGeom->numSpines)
			pool_resize_list(header->spines, header->numSamples * header->procGeom->numSpineVtx, 16);
		if (header->procGeom->deform)
			pool_resize_list(header->deform_vertices, header->numSamples * header->procGeom->deform->nvertices, 16);
	}
#if MMRM_USE_JOB_SYSTEM
	TMergedMesh_InitializeSpines job;
	job.SetPriorityLevel(JobManager::eLowPriority);
	job.SetClassInstance(this);
	job.RegisterJobState(&m_spineInitializationState);
	job.Run();
#else
	InitializeSpines();
#endif
}

void CMergedMeshRenderNode::RemoveSpines()
{
	if (m_spineInitializationState.IsRunning() || m_updateState.IsRunning())
		return;
	if (m_State != STREAMED_IN || !m_SpinesActive)
		return;
	for (size_t i = 0; i < NumGroups(); ++i)
	{
		SMMRMGroupHeader* header = &m_groups[i];
		if (header->spines) pool_free(header->spines);
		if (header->deform_vertices) pool_free(header->deform_vertices);
		header->spines = NULL;
		header->deform_vertices = NULL;
	}
	m_SpinesActive = false;
}

void CMergedMeshRenderNode::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	if (nError != 0u || !pStream)
	{
		return;
	}

	if (m_State != STREAMING)
	{
		CryLogAlways("StreamAsyncOnComplete state check failed (%d) (THIS SHOULD NEVER HAPPEN)!\n", m_State);
		return;
	}

	const float fExtents = c_MergedMeshesExtent;
	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;
	size_t stepcount = 0u;

	unsigned int nSize = pStream->GetBytesRead();
	const uint8* pBuffer = reinterpret_cast<const uint8*>(pStream->GetBuffer());
	if (nSize == 0u || pBuffer == NULL)
	{
		CryLogAlways("%s: zero sized block or null buffer?", __FUNCTION__);
		return;
	}

	for (size_t i = 0; i < NumGroups(); ++i)
	{
		SMMRMGroupHeader* header = &m_groups[i];
		if (header->splitGroup == false && stepcount)
		{
			pBuffer += sizeof(SMergedMeshInstanceCompressed) * stepcount;
			stepcount = 0;
		}
		if (header->splitGroup == false)
		{
			const SMergedMeshSectorChunk& sectorChunk = *StepData<SMergedMeshSectorChunk>(pBuffer, eLittleEndian);

			if (!(sectorChunk.ver == c_MergedMeshChunkVersion))
			{
				CryLogAlways("ERRROR: outdated pvrn chunkformat found, please re-export level!");
				m_State = RENDERNODE_STATE_ERROR;
				continue;
			}

#if MMRM_DEBUG
			{
				if (sectorChunk.m_StatInstGroupID != header->instGroupId) __debugbreak();
				if (sectorChunk.i != (uint32)floorf(abs(m_initPos.x) * fExtentsRec)) __debugbreak();
				if (sectorChunk.j != (uint32)floorf(abs(m_initPos.y) * fExtentsRec)) __debugbreak();
				if (sectorChunk.k != (uint32)floorf(abs(m_initPos.z) * fExtentsRec)) __debugbreak();
			}
#endif
		}

		if (!header->specMismatch)
			pool_resize_list(header->instances, header->numSamplesAlloc = header->numSamples, 16);

		stepcount += header->numSamples;
	}

	TMergedMesh_InitializeSamples job(fExtents, reinterpret_cast<const uint8*>(pStream->GetBuffer()));
	job.SetPriorityLevel(JobManager::eLowPriority);
	job.SetClassInstance(this);
	job.Run();
}

void CMergedMeshRenderNode::PrintState(float& yPos)
{
	int i, j, k;
	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;

	i = (int)floorf(abs(m_pos.x) * fExtentsRec);
	j = (int)floorf(abs(m_pos.y) * fExtentsRec);
	k = (int)floorf(abs(m_pos.z) * fExtentsRec);
	Cry3DEngineBase::Get3DEngine()->DrawTextLeftAligned(10.f, yPos += 13.f, 1.f, Col_Red
	                                                    , "mmrm__%02d_%02d_%02d \t %5s \t %9.3f \t %11s \t %8.3f kb \t %8d"
	                                                    , (unsigned int)i, (unsigned int)j, (unsigned int)k
	                                                    , (Visible() ? "true" : "false")
	                                                    , DistanceSq()
	                                                    , ToString(m_State)
	                                                    , MetaSize() / 1024.f
	                                                    , m_LastDrawFrame);
}

void CMergedMeshRenderNode::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	if (nError != 0u || !pStream)
		return;

	if (m_State != STREAMING)
	{
		CryLogAlways("StreamOnComplete state check failed (%d) (THIS SHOULD NEVER HAPPEN)\n", m_State);
		m_State = RENDERNODE_STATE_ERROR;
		return;
	}
	m_pReadStream->FreeTemporaryMemory();
	m_pReadStream = NULL;
	m_State = STREAMED_IN;
}

bool CMergedMeshRenderNode::StreamIn()
{
	int x = 0, y = 0;
	int i, j, k;
	char szFileName[_MAX_PATH];
	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;

	switch (m_State)
	{
	case PREPARED:
		i = (int)floorf(abs(m_initPos.x) * fExtentsRec);
		j = (int)floorf(abs(m_initPos.y) * fExtentsRec);
		k = (int)floorf(abs(m_initPos.z) * fExtentsRec);
		m_State = STREAMING;
		cry_sprintf(szFileName, "%s%s\\sector_%d_%d_%d.dat", Get3DEngine()->GetLevelFolder(), COMPILED_MERGED_MESHES_BASE_NAME, i, j, k);
		m_pReadStream = GetSystem()->GetStreamEngine()->StartRead(eStreamTaskTypeMergedMesh, szFileName, this);
		return true;
	default:
		break;
	}
	return false;
}

bool CMergedMeshRenderNode::StreamOut()
{
	switch (m_State)
	{
	case STREAMED_IN:
		if (!SyncAllJobs())
			return false;
		DeleteRenderMesh(RUT_STATIC, true, true);
		DeleteRenderMesh(RUT_DYNAMIC, true, true);
		m_State = PREPARED;
		for (size_t i = 0; i < NumGroups(); ++i)
		{
			SMMRMGroupHeader* header = &m_groups[i];
			if (header->instances) pool_free(header->instances);
			if (header->spines) pool_free(header->spines);
			if (header->deform_vertices) pool_free(header->deform_vertices);
			header->numSamplesAlloc = 0;
			header->spines = NULL;
			header->deform_vertices = NULL;
			header->instances = NULL;
		}
		if (m_wind) { CryModuleMemalignFree(m_wind); m_wind = NULL; }
		if (m_density) { CryModuleMemalignFree(m_density); m_density = NULL; }
		if (m_Colliders) { CryModuleMemalignFree(m_Colliders); m_Colliders = NULL; }
		m_SpinesActive = false;
		return true;
	default:
		break;
	}
	return false;
}

void CMergedMeshRenderNode::DebugRender(int nLod)
{
#if MMRM_RENDER_DEBUG
	const float fExtents = c_MergedMeshesExtent;
	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;
	Vec3 extents = (m_visibleAABB.max - m_visibleAABB.min) * 0.5f;
	Vec3 origin = m_pos - extents;
	Quat q;

	IRenderAuxGeom* pAuxGeomRender = gEnv->pRenderer->GetIRenderAuxGeom();
	int mergedMeshesDebug = GetCVars()->e_MergedMeshesDebug;
	if (mergedMeshesDebug > 0)
	{
		Matrix34 mat;
		mat.SetIdentity();
		ColorF col;
		if (mergedMeshesDebug & (1 << 1))
		{
			switch (m_State)
			{
			case STREAMED_IN:
				pAuxGeomRender->DrawAABB(m_internalAABB, mat, false, Col_Yellow, eBBD_Extremes_Color_Encoded);
				switch (m_RenderMode)
				{
				case DYNAMIC:
					pAuxGeomRender->DrawAABB(m_visibleAABB, mat, false, Col_Red, eBBD_Extremes_Color_Encoded);
					break;
				case INSTANCED:
					col.lerpFloat(Col_Red, Col_Green, (float)m_nLod / (float)MAX_STATOBJ_LODS_NUM);
					pAuxGeomRender->DrawAABB(m_visibleAABB, mat, false, Col_Green, eBBD_Extremes_Color_Encoded);
					break;
				}
				break;
			default:
				pAuxGeomRender->DrawAABB(m_internalAABB, mat, false, Col_Magenta, eBBD_Extremes_Color_Encoded);
				break;
			}
			uint32 nInds = 0, nVerts = 0, nChunks = 0;
			for (size_t t = 0; t < 2; ++t)
				for (size_t i = 0; i < m_renderMeshes[t].size(); ++i)
				{
					nInds += m_renderMeshes[t][i].indices;
					nVerts += m_renderMeshes[t][i].vertices;
					nChunks += m_renderMeshes[t][i].chunks;
				}
			IRenderAuxText::DrawLabelF(m_pos, 1.2f, "vb %d ib %d\nsize %3.1f kb\nlod %d ch %d"
			                           , nVerts
			                           , nInds
			                           , (m_SizeInVRam) / 1024.f
			                           , nLod
			                           , nChunks);
		}
	}
	if (m_State != STREAMED_IN)
		return;
	if (mergedMeshesDebug & (1 << 3))
	{
		std::vector<Vec3> lines;
		std::vector<vtx_idx> indices;
		Matrix34 wmat;
		wmat.SetIdentity();
		for (size_t i = 0; i < m_nGroups; ++i)
		{
			const SMMRMGroupHeader& header = m_groups[i];
			for (size_t l = 0; l < header.numSamples; ++l)
			{
				Vec3 v;
				ConvertInstanceAbsolute(v, header.instances[l], m_internalAABB.min, m_pos, m_zRotation, fExtents);
				float scale = (1.f / VEGETATION_CONV_FACTOR) * header.instances[l].scale;

				indices.push_back(lines.size());
				lines.push_back(v);

				indices.push_back(lines.size());
				lines.push_back(v + Vec3(0, 0, scale));
			}
		}
		if (!lines.empty() && !indices.empty())
			pAuxGeomRender->DrawLines(
			  &lines[0], lines.size(), &indices[0], indices.size(), Col_Green, 1.f);
	}
	if (GetCVars()->e_MergedMeshesDebug & (1 << 4))
	{
		std::vector<AABB> aabbs;
		for (size_t i = 0; i < m_nGroups; ++i)
		{
			const SMMRMGroupHeader& header = m_groups[i];
			const AABB aabb = GetStatObj(m_groups[i].instGroupId)->GetAABB();
			for (size_t l = 0; l < header.numSamples; ++l)
			{
				Vec3 pos;
				ConvertInstanceAbsolute(pos, header.instances[l], m_internalAABB.min, m_pos, m_zRotation, fExtents);
				float scale = (1.f / VEGETATION_CONV_FACTOR) * header.instances[l].scale;
				aabbs.push_back(AABB(pos + aabb.min * scale, pos + aabb.max * scale));
			}
		}
		if (!aabbs.empty())
			pAuxGeomRender->DrawAABBs(
			  &aabbs[0], aabbs.size(), false, Col_Green, eBBD_Extremes_Color_Encoded);
	}
	if (GetCVars()->e_MergedMeshesDebug & (1 << 5))
	{
		std::vector<Vec3> lines0, lines1;
		std::vector<vtx_idx> indices0, indices1;
		for (size_t i = 0; i < m_nGroups; ++i)
		{
			const SMMRMGroupHeader& header = m_groups[i];

			if (GetCVars()->e_MergedMeshesDebug & (1 << 9))
				for (size_t l = 0; header.spines && l < header.numSamples; ++l)
				{
					const SMMRMInstance& sample = header.instances[l];
					const float fScale = (1.f / VEGETATION_CONV_FACTOR) * sample.scale;
					DecompressQuat(q, sample);
					Matrix34 wmat = CreateRotationQ(q, ConvertInstanceAbsolute(sample, m_internalAABB.min, m_pos, m_zRotation, fExtents)) * Matrix34::CreateScale(Vec3(fScale, fScale, fScale));
					for (size_t j = 0, base = 0; j < header.procGeom->numSpines; ++j)
					{
						for (size_t k = 0; k < header.procGeom->pSpineInfo[j].nSpineVtx - 1; ++k)
						{
							indices0.push_back(lines0.size());
							lines0.push_back(wmat * header.procGeom->pSpineVtx[base + k].pt);
							indices0.push_back(lines0.size());
							lines0.push_back(wmat * header.procGeom->pSpineVtx[base + k + 1].pt);
						}
						base += header.procGeom->pSpineInfo[j].nSpineVtx;
					}
				}
			if (GetCVars()->e_MergedMeshesDebug & (1 << 10))
			{
				for (size_t l = 0, sbase = 0; header.spines && l < header.numSamples; ++l)
				{
					const SMMRMInstance& sample = header.instances[l];
					const float fScale = (1.f / VEGETATION_CONV_FACTOR) * sample.scale;
					DecompressQuat(q, sample);
					Matrix34 wmat = CreateRotationQ(q, ConvertInstanceAbsolute(sample, m_internalAABB.min, m_pos, m_zRotation, fExtents)) * Matrix34::CreateScale(Vec3(fScale, fScale, fScale));
					for (size_t j = 0, base = 0; j < header.procGeom->numSpines; ++j)
					{
						for (size_t k = 0; k < header.procGeom->pSpineInfo[j].nSpineVtx - 1; ++k)
						{
	#if MMRM_SIMULATION_USES_FP32
							indices1.push_back(lines1.size());
							lines1.push_back(header.spines[sbase + base + k].pt);
							indices1.push_back(lines1.size());
							lines1.push_back(header.spines[sbase + base + k + 1].pt);

							indices1.push_back(lines1.size());
							lines1.push_back(header.spines[sbase + base + k + 1].pt);
							indices1.push_back(lines1.size());
							lines1.push_back(header.spines[sbase + base + k + 1].pt + header.spines[sbase + base + k + 1].vel);
	#else
							indices1.push_back(lines1.size());
							lines1.push_back(wmat * header.spines[sbase + base + k].pt.ToVec3());
							indices1.push_back(lines1.size());
							lines1.push_back(wmat * header.spines[sbase + base + k + 1].pt.ToVec3());

							indices1.push_back(lines1.size());
							lines1.push_back(wmat * header.spines[sbase + base + k + 1].pt.ToVec3());
							indices1.push_back(lines1.size());
							lines1.push_back(wmat * header.spines[sbase + base + k + 1].pt.ToVec3() + header.spines[sbase + base + k + 1].vel.ToVec3());
	#endif
						}
						base += header.procGeom->pSpineInfo[j].nSpineVtx;
					}
					sbase += header.procGeom->numSpineVtx;
				}
			}
			if (GetCVars()->e_MergedMeshesDebug & (1 << 11))
				for (size_t l = 0, sbase = 0; header.spines && l < header.numSamples; ++l)
				{
					const SMMRMInstance& sample = header.instances[l];
					ColorB col;
					if (sample.lastLod == -1)
						col = Col_Blue;
					else
						col.lerpFloat(Col_Red, Col_Green, (float)sample.lastLod / (float)MAX_STATOBJ_LODS_NUM);
					for (size_t j = 0, base = 0; j < header.procGeom->numSpines; ++j)
					{
						for (size_t k = 0; k < header.procGeom->pSpineInfo[j].nSpineVtx - 1; ++k)
						{
	#if MMRM_SIMULATION_USES_FP32
							pAuxGeomRender->DrawLine(
							  header.spines[sbase + base + k].pt, col, header.spines[sbase + base + k + 1].pt, col, 1.f);
	#else
							pAuxGeomRender->DrawLine(
							  header.spines[sbase + base + k].pt.ToVec3(), col, header.spines[sbase + base + k + 1].pt.ToVec3(), col, 1.f);
	#endif
						}
						base += header.procGeom->pSpineInfo[j].nSpineVtx;
					}
					sbase += header.procGeom->numSpineVtx;
				}
		}
		if (!lines0.empty() && !indices0.empty())
			pAuxGeomRender->DrawLines(
			  &lines0[0], lines0.size(), &indices0[0], indices0.size(), Col_Green, 1.f);
		if (!lines1.empty() && !indices1.empty())
			pAuxGeomRender->DrawLines(
			  &lines1[0], lines1.size(), &indices1[0], indices1.size(), Col_Red, 1.f);
	}
	if (GetCVars()->e_MergedMeshesDebug & (1 << 6) && m_wind)
	{
		for (size_t x = 0; x < MMRM_WIND_DIM; ++x)
			for (size_t y = 0; y < MMRM_WIND_DIM; ++y)
				for (size_t z = 0; z < MMRM_WIND_DIM; ++z)
				{
					Vec3 pos, w = m_wind[x + y * MMRM_WIND_DIM + z * MMRM_WIND_DIM * MMRM_WIND_DIM].GetNormalized(), size = m_internalAABB.GetSize();
					pos.x = m_internalAABB.min.x + (x / (MMRM_WIND_DIM - 1.f)) * size.x;
					pos.y = m_internalAABB.min.y + (y / (MMRM_WIND_DIM - 1.f)) * size.y;
					pos.z = m_internalAABB.min.z + (z / (MMRM_WIND_DIM - 1.f)) * size.z;
					pAuxGeomRender->DrawLine(pos, Col_Blue, pos + w, Col_Blue, 4.f);
					pAuxGeomRender->DrawCone(pos + w, w, 0.1f, 0.25f, Col_LightBlue);
				}
	}
	#if MMRM_VISUALIZE_WINDFIELD
	VisualizeWindField(m_wind, m_internalAABB, true, true);
	#endif
	if (GetCVars()->e_MergedMeshesDebug & (1 << 8) && m_nColliders)
	{
		SAuxGeomRenderFlags oldFlags = pAuxGeomRender->GetRenderFlags(), newFlags = oldFlags;
		newFlags.SetFillMode(e_FillModeWireframe);
		pAuxGeomRender->SetRenderFlags(newFlags);
		for (int i = 0; i < m_nColliders; ++i)
		{
			primitives::sphere& sphere = m_Colliders[i];
			pAuxGeomRender->DrawSphere(sphere.center, sphere.r, Col_Pink, true);
		}
		pAuxGeomRender->SetRenderFlags(oldFlags);
	}
#endif
}

const SMMRMGroupHeader* CMergedMeshRenderNode::Group(size_t index) const
{
	return &m_groups[index];
}

uint32 CMergedMeshRenderNode::NumGroups() const
{
	return m_nGroups;
}

uint32 CMergedMeshRenderNode::MetaSize() const
{
	uint32 size = 0u;
	for (size_t i = 0; i < m_nGroups; ++i)
	{
		size += sizeof(SMMRMGroupHeader);
		if (m_groups[i].specMismatch)
			continue;
		size += sizeof(SMMRMInstance) * m_groups[i].numSamples;
		if (m_groups[i].procGeom->numSpines)
			size += sizeof(SMMRMSpineVtx) * m_groups[i].numSamples * m_groups[i].procGeom->numSpineVtx;
		size += sizeof(SMMRMVisibleChunk) * m_groups[i].numVisbleChunks;
	}
	return size;
}

uint32 CMergedMeshRenderNode::VisibleInstances(uint32 frameId) const
{
	if (m_LastDrawFrame != frameId)
		return 0u;

	uint32 count = 0u;
	for (size_t i = 0; i < m_nGroups; ++i)
		count += m_groups[i].numSamplesVisible;
	return count;
}

uint32 CMergedMeshRenderNode::SpineCount() const
{
	IF (gEnv->IsEditor() || m_SpineCount == (size_t)-1, 0)
	{
		uint32 count = 0u;
		bool all_prepared = true;
		for (size_t i = 0; i < m_nGroups; ++i)
		{
			all_prepared &= (m_groups[i].procGeom->state == SMMRMGeometry::PREPARED);
			if (!m_groups[i].specMismatch && m_groups[i].procGeom->numSpines)
			{
				count += m_groups[i].procGeom->numSpineVtx * m_groups[i].numSamples;
			}
		}
		IF (all_prepared, 1)
			m_SpineCount = count;
		return (count);
	}
	return m_SpineCount;
}

uint32 CMergedMeshRenderNode::DeformCount() const
{
	IF (gEnv->IsEditor() || m_DeformCount == (size_t)-1, 0)
	{
		uint32 count = 0u;
		bool all_prepared = true;
		for (size_t i = 0; i < m_nGroups; ++i)
		{
			all_prepared &= (m_groups[i].procGeom->state == SMMRMGeometry::PREPARED);
			if (!m_groups[i].specMismatch && m_groups[i].procGeom->deform)
			{
				count += m_groups[i].procGeom->numVtx;
			}
		}
		IF (all_prepared, 1)
			m_DeformCount = count;
		return count;
	}
	return m_DeformCount;
}

void CMergedMeshRenderNode::OverrideViewDistRatio(float value)
{
	for (size_t i = 0; i < m_nGroups; ++i)
	{
		StatInstGroup* pInstGroup = &GetStatObjGroup(m_groups[i].instGroupId);
		m_groups[i].maxViewDistance = pInstGroup->fVegRadius * pInstGroup->fMaxViewDistRatio * value;
	}
}

void CMergedMeshRenderNode::OverrideLodRatio(float value)
{
	for (size_t i = 0; i < m_nGroups; ++i)
	{
		m_groups[i].lodRationNorm = GetStatObjGroup(m_groups[i].instGroupId).fLodDistRatio;
	}

}

void CMergedMeshRenderNode::UpdateStreamingPriority(const SUpdateStreamingPriorityContext& context)
{
	CObjManager* pObjManager = GetObjManager();
	IF (m_usedMaterials.size() == 0u, 0)
	{
		for (size_t i = 0; i < m_nGroups; ++i)
		{
			if (!m_groups[i].procGeom)
				continue;
			IMaterial* material = NULL;
			IStatObj* pObj = NULL;
			if (m_groups[i].procGeom->is_obj)
				material = (pObj = m_groups[i].procGeom->srcObj)->GetMaterial();
			else if (pObj = GetStatObj(m_groups[i].instGroupId))
			{
				StatInstGroup* pInstGroup = &GetStatObjGroup(m_groups[i].instGroupId);
				material = pInstGroup->pMaterial
				           ? pInstGroup->pMaterial.get()
				           : const_cast<IMaterial*>(pInstGroup->pStatObj->GetMaterial());
			}
			if (material)
				m_usedMaterials.push_back(std::make_pair(material, pObj));
		}
	}
	for (size_t i = 0; i < m_usedMaterials.size(); ++i)
	{
		pObjManager->PrecacheStatObjMaterial(
		  m_usedMaterials[i].first
		  , context.distance
		  , m_usedMaterials[i].second
		  , context.bFullUpdate, false);
	}
}

bool CMergedMeshRenderNode::SyncAllJobs()
{
	IF (m_cullState.IsRunning(), 0)
		return false;
	IF (m_updateState.IsRunning(), 0)
		return false;
	IF (m_spineInitializationState.IsRunning(), 0)
		return false;
	return true;
}

Vec3 CMergedMeshRenderNode::GetSamplePos(size_t group, size_t sample) const
{
	const SMMRMGroupHeader* header;
	if (group >= m_nGroups)
		goto error;
	header = &m_groups[group];
	if (sample >= header->numSamples)
		goto error;
	return ConvertInstanceAbsolute(header->instances[sample], m_internalAABB.min, m_pos, m_zRotation, c_MergedMeshesExtent);
error:
	__debugbreak();
	return Vec3(-1, -1, -1);
}

AABB CMergedMeshRenderNode::GetSampleAABB(size_t group, size_t sample) const
{
	AABB bb;
	float fScale;
	Quat q;
	const SMMRMGroupHeader* header;
	if (group >= m_nGroups)
		goto error;
	header = &m_groups[group];
	if (sample >= header->numSamples)
		goto error;
	fScale = (1.f / VEGETATION_CONV_FACTOR) * header->instances[sample].scale;
	DecompressQuat(q, header->instances[sample]);
	bb = AABB::CreateTransformedAABB(CreateRotationQ(q, ConvertInstanceAbsolute(header->instances[sample], m_internalAABB.min, m_pos, m_zRotation, c_MergedMeshesExtent)) * Matrix34::CreateScale(Vec3(fScale, fScale, fScale)), header->procGeom->aabb);
	return bb;
error:
	__debugbreak();
	return AABB();
}

bool CMergedMeshRenderNode::Visible() const
{
	return (s_mmrm_globals.frameId == m_LastDrawFrame);
}

void CMergedMeshRenderNode::Reset()
{
	for (size_t i = 0; i < m_nGroups; ++i)
		if (m_groups[i].procGeom)
			gEnv->pJobManager->WaitForJob(m_groups[i].procGeom->geomPrepareState);
	gEnv->pJobManager->WaitForJob(m_cullState);
	gEnv->pJobManager->WaitForJob(m_updateState);

	m_renderMeshes[0].clear();
	m_renderMeshes[1].clear();
	m_State = DIRTY;
}

CMergedMeshInstanceProxy::CMergedMeshInstanceProxy(
  CMergedMeshRenderNode* host
  , size_t header
  , size_t index)
	: IRenderNode()
	, m_host(host)
	, m_headerIndex(header)
	, m_sampleIndex(index)
{
}

CMergedMeshInstanceProxy::~CMergedMeshInstanceProxy()
{
	MEMORY_SCOPE_CHECK_HEAP();
	if (m_host && m_host->RemoveInstance(m_headerIndex, m_sampleIndex) == 0)
	{
		m_host->ReleaseNode();
	}
}

CMergedMeshesManager::CMergedMeshesManager()
	: m_ProjectileLock()
	, m_CurrentSizeInVramDynamic()
	, m_CurrentSizeInVramInstanced()
	, m_CurrentSizeInMainMem()
	, m_GeomSizeInMainMem()
	, m_InstanceCount()
	, m_VisibleInstances()
	, m_InstanceSize()
	, m_SpineSize()
	, m_nActiveNodes()
	, m_nStreamedOutNodes()
	, m_PoolOverFlow()
	, m_MeshListPresent()
{
	memset(m_CachedBBs, 0, sizeof(m_CachedBBs));
	memset(m_CachedLRU, 0, sizeof(m_CachedLRU));
	memset(m_CachedNodes, 0, sizeof(m_CachedNodes));
}
CMergedMeshesManager::~CMergedMeshesManager()
{
}

void CMergedMeshesManager::Init()
{
	s_GeomManager.Initialize(gEnv->pJobManager->GetNumWorkerThreads());
	if (ICVar* pVar = gEnv->pConsole->GetCVar("e_MergedMeshesLodRatio"))
		pVar->SetOnChangeCallback(UpdateRatios);
	if (ICVar* pVar = gEnv->pConsole->GetCVar("e_MergedMeshesViewDistRatio"))
		pVar->SetOnChangeCallback(UpdateRatios);

	if (!s_MergedMeshPool)
	{
		AUTO_LOCK(s_MergedMeshPoolLock);
		size_t memsize = (Cry3DEngineBase::GetCVars()->e_MergedMeshesPool + 4096) * 1024; // include 2mb buffer size for overhead and large peaks
		if (memsize && (s_MergedMeshPool = gEnv->pSystem->GetIMemoryManager()->CreateGeneralExpandingMemoryHeap(
		                  memsize, 0, "MERGEDMESH_POOL")) == NULL)
		{
			CryFatalError("could not allocate mergedmeshpoolheap");
		}
	}

	// Preallocate to prevent reallocations
	m_ActiveNodes.reserve(8192);
	m_StreamedOutNodes.reserve(8192);
	m_VisibleNodes.reserve(8192);

	gEnv->pPhysicalWorld->AddEventClient(EventPhysPostStep::id, CMergedMeshesManager::OnPhysPostStep, 0, 1.0f);
}
void CMergedMeshesManager::Shutdown()
{
	MEMORY_SCOPE_CHECK_HEAP();
	if (ICVar* pVar = gEnv->pConsole->GetCVar("e_MergedMeshesLodRatio"))
		pVar->SetOnChangeCallback(NULL);
	if (ICVar* pVar = gEnv->pConsole->GetCVar("e_MergedMeshesViewDistRatio"))
		pVar->SetOnChangeCallback(NULL);
	for (size_t i = 0; i < HashDimXY; ++i)
		for (size_t j = 0; j < HashDimXY; ++j)
			for (size_t k = 0; k < HashDimZ; ++k)
			{
				NodeListT& list = m_Nodes[i][j][k];
				if (!list.empty())
					CryFatalError("Node list not empty. (THIS SHOULD NEVER HAPPEN)");
				stl::free_container(list);
			}
	s_GeomManager.Shutdown();
	stl::free_container(m_ActiveNodes);
	stl::free_container(m_StreamedOutNodes);
	stl::free_container(m_VisibleNodes);
	stl::free_container(m_Projectiles);
	stl::free_container(m_PostRenderNodes);

	if (s_MergedMeshPool)
	{
		AUTO_LOCK(s_MergedMeshPoolLock);
		s_MergedMeshPool->Cleanup();
	}
	m_MeshListPresent = false;

	gEnv->pPhysicalWorld->RemoveEventClient(EventPhysPostStep::id, CMergedMeshesManager::OnPhysPostStep, 0);
}

void CMergedMeshesManager::UpdateViewDistRatio(float value)
{
	for (size_t i = 0; i < HashDimXY; ++i)
		for (size_t j = 0; j < HashDimXY; ++j)
			for (size_t k = 0; k < HashDimZ; ++k)
			{
				NodeListT& list = m_Nodes[i][j][k];
				for (NodeListT::iterator it = list.begin(); it != list.end(); ++it)
					(*it)->OverrideViewDistRatio(value);
			}
}

void CMergedMeshesManager::UpdateLodRatio(float value)
{
	for (size_t i = 0; i < HashDimXY; ++i)
		for (size_t j = 0; j < HashDimXY; ++j)
			for (size_t k = 0; k < HashDimZ; ++k)
			{
				NodeListT& list = m_Nodes[i][j][k];
				for (NodeListT::iterator it = list.begin(); it != list.end(); ++it)
					(*it)->OverrideLodRatio(value);
			}
}

bool CMergedMeshesManager::CompileSectors(std::vector<struct IStatInstGroup*>* pVegGroupTable)
{
	m_InstanceSectors.clear();
	int nSize = 0;
	for (size_t i = 0; i < HashDimXY; ++i)
		for (size_t j = 0; j < HashDimXY; ++j)
			for (size_t k = 0; k < HashDimZ; ++k)
			{
				NodeListT& list = m_Nodes[i][j][k];
				for (NodeListT::iterator it = list.begin(); it != list.end(); ++it)
				{
					nSize = 0;
					SInstanceSector* pSector = m_InstanceSectors.push_back();
					(*it)->Compile(NULL, nSize, NULL, NULL);
					pSector->data.resize(nSize);

					if ((*it)->Compile(&pSector->data[0], nSize, &pSector->id, pVegGroupTable) == false)
						return false;
				}
			}
	return true;
}

void CMergedMeshesManager::ClearInstanceSectors()
{
	m_InstanceSectors.clear();
}

bool CMergedMeshesManager::CompileAreas(DynArray<SMeshAreaCluster>& clusters, int flags)
{
	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;

	typedef std::unordered_map<CMergedMeshRenderNode*, size_t> ClusterMappingT;
	ClusterMappingT mapping;
	NodeArrayT list, stack;
	CryLogAlways("MMRM: clustering ...");

	// Extract a list of all merged mesh nodes
	for (size_t i = 0; i < HashDimXY; ++i)
		for (size_t j = 0; j < HashDimXY; ++j)
			for (size_t k = 0; k < HashDimZ; ++k)
			{
				NodeListT& _list = m_Nodes[i][j][k];
				for (NodeListT::iterator it = _list.begin(); it != _list.end(); ++it)
					list.push_back(*it);
			}

	// No nodes means no areas - we are done
	if (list.size() == 0)
		return true;
	CryLogAlways("MMRM: processing %d nodes ...", (int)list.size());

	// Pick a node from the list and create a cluster. Perform a breadth-first-search on every
	// node and collect all neighbours reachable from the starting node to the cluster. Mark
	// already visited nodes in a hash table to skip them efficiently.
	for (size_t c = list.size(); c >= 1; --c)
	{
		ClusterMappingT::iterator found = mapping.find(list[c - 1]);
		if (found != mapping.end())
			continue;

		size_t cluster_index = clusters.size();
		clusters.push_back(SMeshAreaCluster());
		clusters[cluster_index].extents.Reset();
		stack.push_back(list[c - 1]);

		while (stack.size())
		{
			CMergedMeshRenderNode* node = stack.back();
			stack.pop_back();

			ClusterMappingT::iterator found = mapping.find(node);
			if (found != mapping.end())
				continue;
			mapping.insert(std::make_pair(node, cluster_index));

			const AABB& box = node->GetInternalBBox();
			const Vec3& centre = box.GetCenter();
			int _i = (int)floorf(abs(centre.x) * fExtentsRec);
			int _j = (int)floorf(abs(centre.y) * fExtentsRec);
			int _k = (int)floorf(abs(centre.z) * fExtentsRec);

			// Note, as this is a wrapping grid, we need to weed non-neighbours
			for (int i = _i - 1; i <= _i + 1; ++i)
				for (int j = _j - 1; j <= _j + 1; ++j)
					for (int k = _k - 1; k <= _k + 1; ++k)
					{
						NodeListT& _list = m_Nodes[i & (HashDimXY - 1)][j & (HashDimXY - 1)][k & (HashDimZ - 1)];
						for (NodeListT::iterator it = _list.begin(); it != _list.end(); ++it)
						{
							const AABB& nbox = (*it)->GetInternalBBox();
							const Vec3& ncentre = nbox.GetCenter();
							int ni = (int)floorf(abs(ncentre.x) * fExtentsRec);
							int nj = (int)floorf(abs(ncentre.y) * fExtentsRec);
							int nk = (int)floorf(abs(ncentre.z) * fExtentsRec);
							if (abs(ni - _i) <= 1 && abs(nj - _j) <= 1 && abs(nk - _k) <= 1)
								stack.push_back(*(it));
						}
					}
		}
	}

	// Let the clusters process all the nodes that were found by them
	CryLogAlways("MMRM: creating %d clusters ...", (int)clusters.size());
	for (ClusterMappingT::iterator it = mapping.begin(), end = mapping.end(); it != end; ++it)
	{
		CMergedMeshRenderNode* node = it->first;
		SMeshAreaCluster& cluster = clusters[it->second];

		const AABB& nodeBB = node->GetInternalBBox();
		cluster.extents.Add(nodeBB);
		if (flags & CLUSTER_BOUNDARY_FROM_SAMPLES)
			node->FillSamples(cluster.boundary_points);
		else
		{
			cluster.boundary_points.push_back(Vec2(nodeBB.min.x, nodeBB.min.y));
			cluster.boundary_points.push_back(Vec2(nodeBB.max.x, nodeBB.min.y));
			cluster.boundary_points.push_back(Vec2(nodeBB.max.x, nodeBB.max.y));
			cluster.boundary_points.push_back(Vec2(nodeBB.min.x, nodeBB.max.y));
		}
	}

	// Now calculate the boundary for each cluster
	CryLogAlways("MMRM: calculating boundaries ...");
	for (size_t i = 0, end = clusters.size(); i < end; ++i)
	{
		if (flags & CLUSTER_CONVEXHULL_GRAHAMSCAN)
		{
			size_t boundaries = convexhull_graham_scan(clusters[i].boundary_points);
			CryLogAlways("MMRM: cluster %d has %d points on it's convex hull", (int)i, (int)boundaries);
			continue;
		}
		if (flags & CLUSTER_CONVEXHULL_GIFTWRAP)
		{
			size_t boundaries = convexhull_giftwrap(clusters[i].boundary_points);
			CryLogAlways("MMRM: cluster %d has %d points on it's convex hull", (int)i, (int)boundaries);
			continue;
		}
		CryFatalError(
		  "MMRM: no clustering algorithm selected, please provide one of the following:\n"
		  "\tCLUSTER_CONVEXHULL_GRAHAMSCAN\n"
		  "\tCLUSTER_CONVEXHULL_GIFTWRAP\n");
	}

#if MMRM_CLUSTER_VISUALIZATION
	if (Cry3DEngineBase::GetCVars()->e_MergedMeshesClusterVisualization > 0)
	{
		m_clusters = clusters;
	}
	else
	{
		m_clusters.clear();
	}
#endif // MMRM_CLUSTER_VISUALIZATION

	CryLogAlways("MMRM: done ...");
	return true;
}

CMergedMeshRenderNode* CMergedMeshesManager::FindNode(const Vec3& pos)
{
	for (size_t i = 0; i < 4; ++i)
	{
		if (m_CachedBBs[i].IsContainPoint(pos))
			return m_CachedNodes[i];
		else
			++m_CachedLRU[i];
	}

	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;
	int i = (int)floorf(abs(pos.x) * fExtentsRec);
	int j = (int)floorf(abs(pos.y) * fExtentsRec);
	int k = (int)floorf(abs(pos.z) * fExtentsRec);

	NodeListT& _list = m_Nodes[i & (HashDimXY - 1)][j & (HashDimXY - 1)][k & (HashDimZ - 1)];
	for (NodeListT::iterator it = _list.begin(); it != _list.end(); ++it)
	{
		CMergedMeshRenderNode* node = *it;
		if (node->m_internalAABB.IsContainPoint(pos))
		{
			int oldest = -INT_MAX, oldidx = 0;
			for (size_t i = 0; i < 4; ++i)
			{
				if (m_CachedLRU[i] > oldest)
				{
					oldidx = i;
					oldest = m_CachedLRU[i];
				}
			}
			m_CachedNodes[oldidx] = node;
			m_CachedBBs[oldidx] = node->m_internalAABB;
			m_CachedLRU[oldidx] = 0;
			return node;
		}
	}

	return NULL;
}

void CMergedMeshesManager::CalculateDensity()
{
	if (gEnv->IsEditor() == false)
		return;
	// Extract a list of all merged mesh nodes
	for (size_t i = 0; i < HashDimXY; ++i)
		for (size_t j = 0; j < HashDimXY; ++j)
			for (size_t k = 0; k < HashDimZ; ++k)
			{
				NodeListT& _list = m_Nodes[i][j][k];
				for (NodeListT::iterator it = _list.begin(); it != _list.end(); ++it)
					(*it)->CalculateDensity();
			}
}

size_t CMergedMeshesManager::QueryDensity(const Vec3& pos, ISurfaceType*(&surfaceTypes)[MMRM_MAX_SURFACE_TYPES], float (&density)[MMRM_MAX_SURFACE_TYPES])
{
	FUNCTION_PROFILER_3DENGINE;

	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;
	int i = (int)(abs(pos.x) * fExtentsRec);
	int j = (int)(abs(pos.y) * fExtentsRec);
	int k = (int)(abs(pos.z) * fExtentsRec);

	NodeListT& _list = m_Nodes[i & (HashDimXY - 1)][j & (HashDimXY - 1)][k & (HashDimZ - 1)];
	for (NodeListT::iterator it = _list.begin(); it != _list.end(); ++it)
	{
		CMergedMeshRenderNode* node = *it;
		if (node->m_internalAABB.IsContainPoint(pos))
			return node->QueryDensity(pos, surfaceTypes, density);
	}

	return 0;
}

bool CMergedMeshesManager::GetUsedMeshes(DynArray<string>& meshNames)
{
	s_GeomManager.GetUsedMeshes(meshNames);
	return true;
}

void CMergedMeshesManager::PreloadMeshes()
{
	LOADING_TIME_PROFILE_SECTION;
	if (FILE* file = gEnv->pCryPak->FOpen(Get3DEngine()->GetLevelFilePath(COMPILED_MERGED_MESHES_BASE_NAME COMPILED_MERGED_MESHES_LIST), "r"))
	{
		gEnv->pCryPak->FSeek(file, 0, SEEK_END);
		size_t length = gEnv->pCryPak->FTell(file);
		gEnv->pCryPak->FSeek(file, 0, SEEK_SET);

		char* contents = new char[length + 1];
		contents[length] = '\0';
		gEnv->pCryPak->FReadRawAll(contents, length, file);

		char* end = contents + length;
		char* iter = contents, * current = contents;

		while (iter < end)
		{
			for (; *iter != '\n' && iter < end; ++iter)
				;
			if (*iter == '\n' && iter != current)
			{
				*iter = '\0';
				CryLog("MMRM loading cgf '%s'", current);
				Get3DEngine()->LoadStatObj(current, NULL, NULL, false);
			}
			current = ++iter;
		}

		delete[] contents;
		gEnv->pCryPak->FClose(file);

		m_MeshListPresent = true;
	}
}

bool CMergedMeshesManager::SyncPreparationStep()
{
	if (m_MeshListPresent)
		return s_GeomManager.SyncPreparationStep();
	return false;
}

IRenderNode* CMergedMeshesManager::AddInstance(const SProcVegSample& sample)
{
	MEMORY_SCOPE_CHECK_HEAP();
	float fExtents = c_MergedMeshesExtent;
	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;
	const Vec3& vPos = sample.pos;

	int i = (int)floorf(abs(vPos.x) * fExtentsRec);
	int j = (int)floorf(abs(vPos.y) * fExtentsRec);
	int k = (int)floorf(abs(vPos.z) * fExtentsRec);

	NodeListT& list = m_Nodes[i & (HashDimXY - 1)][j & (HashDimXY - 1)][k & (HashDimZ - 1)];
	CMergedMeshRenderNode* node = NULL;
	for (NodeListT::iterator it = list.begin(); it != list.end(); ++it)
	{
		if ((*it)->GetInternalBBox().IsContainPoint(vPos))
		{
			node = *it;
			break;
		}
	}
	if (!node)
	{
		Vec3 pos = Vec3(i * fExtents, j * fExtents, k * fExtents);
		Vec3 extents = Vec3(fExtents, fExtents, fExtents);
		list.push_back(node = new CMergedMeshRenderNode());

		AABB box;
		box.max = pos + extents;
		box.min = pos;

		node->Setup(box, AABB(AABB::RESET), NULL);
		node->SetActive(1);
		m_ActiveNodes.push_back(node);
	}
	return node->AddInstance(sample);
}

CMergedMeshRenderNode* CMergedMeshesManager::GetNode(const Vec3& vPos)
{
	float fExtents = c_MergedMeshesExtent;
	float fExtentsRec = 1.f / c_MergedMeshesExtent;

	int i = (int)floorf(abs(vPos.x) * fExtentsRec);
	int j = (int)floorf(abs(vPos.y) * fExtentsRec);
	int k = (int)floorf(abs(vPos.z) * fExtentsRec);

	CMergedMeshRenderNode* node = NULL;
	NodeListT& list = m_Nodes[i & (HashDimXY - 1)][j & (HashDimXY - 1)][k & (HashDimZ - 1)];
	for (NodeListT::iterator it = list.begin(); it != list.end(); ++it)
	{
		if ((*it)->GetInternalBBox().IsContainPoint(vPos))
		{
			node = *it;
			break;
		}
	}
	if (!node)
	{
		list.push_back(node = new CMergedMeshRenderNode());

		Vec3 pos = Vec3(i * fExtents, j * fExtents, k * fExtents);
		Vec3 extents = Vec3(fExtents, fExtents, fExtents);

		AABB box;
		box.max = pos + extents;
		box.min = pos;
		node->Setup(box, AABB(AABB::RESET), NULL);
		node->SetActive(1);
		m_ActiveNodes.push_back(node);
	}

	return node;
}

void CMergedMeshesManager::RemoveMergedMesh(CMergedMeshRenderNode* node)
{
	MEMORY_SCOPE_CHECK_HEAP();
	const float fExtentsRec = 1.0f / c_MergedMeshesExtent;
	const Vec3& vPos = node->m_pos;

	int i = (int)floorf(abs(vPos.x) * fExtentsRec);
	int j = (int)floorf(abs(vPos.y) * fExtentsRec);
	int k = (int)floorf(abs(vPos.z) * fExtentsRec);

	NodeListT& list = m_Nodes[i & (HashDimXY - 1)][j & (HashDimXY - 1)][k & (HashDimZ - 1)];
	assert(std::find(list.begin(), list.end(), node) != list.end());
	list.erase(std::remove(list.begin(), list.end(), node), list.end());

	// Make sure it's deleted from the active list in the editor
	m_ActiveNodes.erase(std::remove(m_ActiveNodes.begin(), m_ActiveNodes.end(), node), m_ActiveNodes.end());
	m_StreamedOutNodes.erase(std::remove(m_StreamedOutNodes.begin(), m_StreamedOutNodes.end(), node), m_StreamedOutNodes.end());
	m_VisibleNodes.erase(std::remove(m_VisibleNodes.begin(), m_VisibleNodes.end(), node), m_VisibleNodes.end());
}

void CMergedMeshesManager::SortActiveInstances(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

#if MMRM_USE_JOB_SYSTEM
	TMMRM_SortActiveInstances job(passInfo);
	job.SetClassInstance(this);
	job.RegisterJobState(&m_updateState);
	job.SetPriorityLevel(JobManager::eHighPriority);
	job.Run();
#else
	SortActiveInstances_Async(passInfo);
#endif
}

void CMergedMeshesManager::Update(const SRenderingPassInfo& passInfo)
{
	CRY_PROFILE_REGION(PROFILE_3DENGINE, "MergedMeshesManager: Update");
	CRYPROFILE_SCOPE_PROFILE_MARKER("MMRMGR: update")

	if (GetCVars()->e_MergedMeshes == 0 || !passInfo.IsGeneralPass())
		return;
	uint32 mainMemLimit = GetCVars()->e_MergedMeshesPool * 1024;
	const uint32 activeSpineLimit = (mainMemLimit * GetCVars()->e_MergedMeshesPoolSpines) / 100;
	mainMemLimit -= activeSpineLimit;
	const uint32 streamInLimit = min(mainMemLimit >> 2, 256u << 10);
	uint32 sumVramSizeDynamic = 0u
	, sumVramSizeInstanced = 0u
	, visibleInstances = 0u
	, nActiveInstances = 0u
	, streamedInSize = 0u
	, metaSize = 0u
	, streamRequests = 0u
	, maxStreamRequests = 16u;
	const int e_MergedMeshesDebug = GetCVars()->e_MergedMeshesDebug;

	const float dt = gEnv->pTimer->GetFrameTime();
	const float sqDiagonalInternal = sqr(c_MergedMeshesExtent) * 1.25f;

	uint32 frameId = passInfo.GetMainFrameID();

	AABB visibleVolume;

	const float fExtents = c_MergedMeshesExtent * 0.25f;
	const Vec3 vDiag = Vec3(fExtents, fExtents, fExtents);
	const float fActiveDist = GetCVars()->e_MergedMeshesActiveDist;
	const float fVisibleDist = fExtents * fActiveDist;
	const Vec3& vPos = passInfo.GetCamera().GetPosition();
	Vec3 vMax, vMin;

	visibleVolume.max = vMax = vPos + Vec3(fVisibleDist, fVisibleDist, fVisibleDist);
	visibleVolume.min = vMin = vPos - Vec3(fVisibleDist, fVisibleDist, fVisibleDist);

#if !defined(_RELEASE)
	m_InstanceSize = 0;
	m_SpineSize = 0;
#endif

	// Update registered particles
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "MMRMGR: update projectiles");
		CRYPROFILE_SCOPE_PROFILE_MARKER("MMRMGR: update projectiles");

		WriteLock _lock(m_ProjectileLock);
		size_t pi = 0, pe = m_Projectiles.size();
		while (pi < pe)
		{
			SProjectile& p = m_Projectiles[pi];
			if ((p.lifetime -= dt) <= 0.f)
			{
				std::swap(m_Projectiles[pi], m_Projectiles[pe - 1]);
				--pe;
				continue;
			}
			++pi;
		}
		m_Projectiles.erase(m_Projectiles.begin() + pe, m_Projectiles.end());
	}

	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "MMRMGR: state jobsync");
		CRYPROFILE_SCOPE_PROFILE_MARKER("MMRMGR: state jobsync");

		// Maintain a sorted list of active instances - we have to wait here for the job to have completed
		gEnv->pJobManager->WaitForJob(m_updateState);
	}

	// Stream in instances up until main memory pool limit
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "MMRMGR: streaming");
		CRYPROFILE_SCOPE_PROFILE_MARKER("MMRMGR: streaming");

		float yPos = 8.f;
		bool hadOverflow = m_PoolOverFlow;
		m_PoolOverFlow = false;
		size_t nActiveNodes = m_ActiveNodes.size();
		size_t nVisibleNodes = m_VisibleNodes.size();

		if (GetCVars()->e_MergedMeshesDebug & 0x2)
		{
			Cry3DEngineBase::Get3DEngine()->DrawTextLeftAligned(10.f, yPos += 13.f, 1.f, Col_White
			                                                    , "Sector(xyz)\tvisible\t\tDistanceSq\tState\t\t\tSize(kb)\t\t\tLast frame drawn");
		}

		for (size_t i = 0, activeSize = 0u; i < nActiveNodes; ++i)
		{
			CMergedMeshRenderNode* node = m_ActiveNodes[i];
			metaSize += node->MetaSize();
#if MMRM_RENDER_DEBUG
			if (GetCVars()->e_MergedMeshesDebug) node->DebugRender(node->m_nLod);
#endif
#if !defined(_RELEASE)
			if (GetCVars()->e_MergedMeshesDebug & 0x2) node->PrintState(yPos);
#endif
			IF (node->m_LastDrawFrame != frameId, 1)
			{
				node->m_SizeInVRam = 0u;
				node->DeleteRenderMesh(CMergedMeshRenderNode::RUT_DYNAMIC);
			}
			IF ((frameId - node->m_LastDrawFrame) > 0x7ffff, 1) // arbitrary high number
			{
				node->m_SizeInVRam = 0u;
				node->DeleteRenderMesh(CMergedMeshRenderNode::RUT_STATIC);
			}
			IF (i + 1 < nActiveNodes, 1) CryPrefetch(m_ActiveNodes[i + 1]);
			if (node->m_State == CMergedMeshRenderNode::INITIALIZED
			    || node->m_State == CMergedMeshRenderNode::DIRTY
			    || node->m_State == CMergedMeshRenderNode::PREPARING)
			{
				if (!(gEnv->IsEditor() || gEnv->IsDedicated()) && node->StreamedIn())
					node->StreamOut();
				continue;
			}

#if !defined(_RELEASE)
			nActiveInstances += node->m_Instances;
			if (e_MergedMeshesDebug)
			{
				for (size_t j = 0; j < node->m_nGroups; ++j)
					visibleInstances += node->m_groups[j].numSamplesVisible;
				for (size_t j = 0; j < node->m_renderMeshes[0].size(); ++j)
					for (size_t k = 0; k < node->m_renderMeshes[0][j].rms.size(); ++k)
						sumVramSizeInstanced += node->m_renderMeshes[0][j].rms[k]->GetMemoryUsage(NULL, IRenderMesh::MEM_USAGE_ONLY_VIDEO);
				for (size_t j = 0; j < node->m_renderMeshes[1].size(); ++j)
					for (size_t k = 0; k < node->m_renderMeshes[1][j].rms.size(); ++k)
						sumVramSizeDynamic += node->m_renderMeshes[1][j].rms[k]->GetMemoryUsage(NULL, IRenderMesh::MEM_USAGE_ONLY_VIDEO);
			}
#endif

			uint32 currentInstSize = node->m_Instances * sizeof(SMMRMInstance);
			if (currentInstSize == 0) // Safe to skip over empty nodes
				continue;
			uint32 currentSpineSize = node->SpineCount() * sizeof(SMMRMSpineVtxBase);
			uint32 currentDeformSize = node->DeformCount() * sizeof(SMMRMDeformVertex);

			if (activeSize + currentInstSize < mainMemLimit)
			{
				if (!(gEnv->IsEditor() || gEnv->IsDedicated()) && !node->StreamedIn() && streamedInSize < streamInLimit && streamRequests < maxStreamRequests)
				{
					if (node->StreamIn())
					{
						streamedInSize += currentInstSize;
						++streamRequests;
					}
				}
				activeSize += currentInstSize;
#if !defined(_RELEASE)
				m_InstanceSize += currentInstSize;
				m_SpineSize += currentSpineSize;
#endif
			}
			else
			{
				if (!(gEnv->IsEditor() || gEnv->IsDedicated()) && node->StreamedIn())
					node->StreamOut();
			}
		}
		for (size_t i = 0, activeSize = 0u; i < nVisibleNodes; ++i)
		{
			CMergedMeshRenderNode* node = m_VisibleNodes[i];
#if 0 && !defined(_RELEASE)
			if (GetCVars()->e_MergedMeshesDebug & 0x2) node->PrintState(yPos);
#endif
			IF (i + 1 < nVisibleNodes, 1) CryPrefetch(m_VisibleNodes[i + 1]);
			if ((hadOverflow && node->m_LastDrawFrame != frameId)
			    || node->m_State == CMergedMeshRenderNode::INITIALIZED
			    || node->m_State == CMergedMeshRenderNode::DIRTY
			    || node->m_State == CMergedMeshRenderNode::PREPARING)
			{
				if (!(gEnv->IsEditor() || gEnv->IsDedicated()))
					node->RemoveSpines();
				node->SetInVisibleSet(0);
				m_VisibleNodes.erase(m_VisibleNodes.begin() + i);
				--nVisibleNodes;
				continue;
			}
			uint32 currentSpineSize = node->SpineCount() * sizeof(SMMRMSpineVtxBase);
			uint32 currentDeformSize = node->DeformCount() * sizeof(SMMRMDeformVertex);

			if (activeSize + currentSpineSize + currentDeformSize < activeSpineLimit)
			{
				if (!(gEnv->IsEditor() || gEnv->IsDedicated()))
					node->ActivateSpines();
				activeSize += currentSpineSize + currentDeformSize;
			}
			else
			{
				if (!(gEnv->IsEditor() || gEnv->IsDedicated()))
					node->RemoveSpines();
				m_PoolOverFlow = true;
			}
		}
	}

#if MMRM_CLUSTER_VISUALIZATION
	if (Cry3DEngineBase::GetCVars()->e_MergedMeshesClusterVisualization > 0)
	{
		for (int i = 0; i < m_clusters.size(); ++i)
		{
			IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
			SAuxGeomRenderFlags old_flags = pAuxGeom->GetRenderFlags();
			SAuxGeomRenderFlags new_flags = old_flags;
			new_flags.SetCullMode(e_CullModeNone);
			new_flags.SetAlphaBlendMode(e_AlphaBlended);
			pAuxGeom->SetRenderFlags(new_flags);

			ColorF col0, col1;
			col0.r = 0.8f;
			col0.g = 0.1f;
			col0.b = 0.2f;
			col0.a = 0.55f;

			col1.r = 0.1f;
			col1.g = 0.9f;
			col1.b = 0.1f;
			col1.a = 0.55f;

			SMeshAreaCluster& cluster = m_clusters[i];
			DynArray<Vec2>& points = cluster.boundary_points;
			size_t npoints = points.size();
			for (size_t j = 1; j < npoints; ++j)
			{
				Vec3 v[4];
				v[0] = Vec3(points[j - 1].x, points[j - 1].y, cluster.extents.min.z);
				v[1] = Vec3(points[j].x, points[j].y, cluster.extents.min.z);
				v[2] = Vec3(points[j].x, points[j].y, cluster.extents.max.z);
				v[3] = Vec3(points[j - 1].x, points[j - 1].y, cluster.extents.max.z);
				pAuxGeom->DrawTriangle(v[0], col0, v[1], col1, v[2], col0);
				pAuxGeom->DrawTriangle(v[2], col0, v[3], col1, v[0], col0);
			}

			Vec3 v[4];
			v[0] = Vec3(points[npoints - 1].x, points[npoints - 1].y, cluster.extents.min.z);
			v[1] = Vec3(points[0].x, points[0].y, cluster.extents.min.z);
			v[2] = Vec3(points[0].x, points[0].y, cluster.extents.max.z);
			v[3] = Vec3(points[npoints - 1].x, points[npoints - 1].y, cluster.extents.max.z);
			pAuxGeom->DrawTriangle(v[0], col0, v[1], col1, v[2], col0);
			pAuxGeom->DrawTriangle(v[2], col0, v[3], col1, v[0], col0);
			pAuxGeom->SetRenderFlags(old_flags);
		}

		if (Cry3DEngineBase::GetCVars()->e_MergedMeshesClusterVisualization > 1 || CryGetAsyncKeyState(0x59))
		{
			Vec3 cpos = passInfo.GetCamera().GetPosition();
			//cpos.x = floorf(cpos.x);
			//cpos.y = floorf(cpos.y);
			//cpos.z = floorf(GetTerrain()->GetZApr(cpos.x, cpos.y, 0));
			float const fDim = static_cast<float>(Cry3DEngineBase::GetCVars()->e_MergedMeshesClusterVisualizationDimension) * 0.5f;
			uint8 const nDim = static_cast<uint8>(Cry3DEngineBase::GetCVars()->e_MergedMeshesClusterVisualizationDimension);
			DisplayDensity(this, AABB(cpos - Vec3(fDim, fDim, fDim), cpos + Vec3(fDim, fDim, fDim)), nDim);
			//DisplayDensitySpheres(this, AABB(cpos-Vec3(fDim, fDim, fDim), cpos+Vec3(fDim, fDim, fDim)), nDim);
		}
	}
#endif // MMRM_CLUSTER_VISUALIZATION

	m_CurrentSizeInVramDynamic = sumVramSizeDynamic;
	m_CurrentSizeInVramInstanced = sumVramSizeInstanced;
	m_CurrentSizeInMainMem = metaSize;
	m_GeomSizeInMainMem = s_GeomManager.Size();
	m_VisibleInstances = visibleInstances;
	m_nActiveNodes = (uint32)m_ActiveNodes.size();
	m_nStreamedOutNodes = (uint32)m_StreamedOutNodes.size();
	m_InstanceCount = nActiveInstances;
}

void CMergedMeshesManager::AddProjectile(const SProjectile& projectile)
{
	WriteLock _lock(m_ProjectileLock);
	// Skip already added entities and only add till end
	if (m_Projectiles.size() >= MaxProjectiles)
		return;
	m_Projectiles.push_back(projectile);
}

int CMergedMeshesManager::OnPhysPostStep(const EventPhys* event)
{
	CMergedMeshesManager* _this = Cry3DEngineBase::m_pMergedMeshesManager;
	const EventPhysPostStep* poststep = static_cast<const EventPhysPostStep*>(event);
	IPhysicalEntity* entity = poststep->pEntity;
	SProjectile projectile;
	pe_status_pos pos;
	pe_params_particle particles;
	if (entity->GetType() != PE_PARTICLE)
		goto skip;

	if (entity->GetStatus(&pos) == 0 || entity->GetParams(&particles) == 0)
		goto skip;

	if (particles.mass > 1.f || particles.size > 0.01f)
		goto skip;

	projectile.entity = entity;
	projectile.current_pos = pos.pos;
	projectile.initial_pos = pos.pos - particles.heading * particles.velocity * poststep->dt;
	projectile.direction = (projectile.current_pos - projectile.initial_pos).GetNormalized();
	projectile.size = (pos.BBox[0] - pos.BBox[1]).len() * Cry3DEngineBase::GetCVars()->e_MergedMeshesBulletScale; //roughly a bullet I'ld say
	projectile.size = max(min(projectile.size, 8.5f), 1.f);
	projectile.lifetime = Cry3DEngineBase::GetCVars()->e_MergedMeshesBulletLifetime;

	_this->AddProjectile(projectile);

skip:
	return 1;
}

void CMergedMeshesManager::RegisterForPostRender(CMergedMeshRenderNode* node)
{
	assert(node);
	m_PostRenderNodes.push_back(node);
}

void CMergedMeshesManager::PostRenderMeshes(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	size_t num_nodes = m_PostRenderNodes.size();
	for (size_t i = 0; i < num_nodes; ++i)
		m_PostRenderNodes[i]->SampleWind();
	for (size_t i = 0; i < num_nodes; ++i)
		m_PostRenderNodes[i]->QueryColliders();
	for (size_t i = 0; i < num_nodes; ++i)
		m_PostRenderNodes[i]->QueryProjectiles();

	// Perform the post render
	do
	{
		bool done = true;
		for (size_t i = 0; i < m_PostRenderNodes.size(); ++i)
		{
			IF (m_PostRenderNodes[i] == NULL, 1)
				continue;
			if (m_PostRenderNodes[i]->PostRender(passInfo) == false)
			{
				m_PostRenderNodes[i] = NULL;
				continue;
			}
			done = false;
		}
		if (done)
		{
			m_PostRenderNodes.clear();
			break;
		}
	}
	while (true);
}

void CMergedMeshesManager::ResetActiveNodes()
{
	for (size_t i = 0; i < HashDimXY; ++i)
		for (size_t j = 0; j < HashDimXY; ++j)
			for (size_t k = 0; k < HashDimZ; ++k)
			{
				NodeListT& list = m_Nodes[i][j][k];
				for (NodeListT::iterator it = list.begin(); it != list.end(); ++it)
					(*it)->Reset();
			}
}

struct SDeformableData
{
	SMMRMGroupHeader   m_mmrmHeader;
	SMMRMUpdateContext m_mmrmContext;
	Matrix34           m_localTM;
	bool               m_needsBaking;
	enum
	{
		CREATED = 0,
		PREPARED,
		READY
	} m_State;

	SDeformableData()
		: m_mmrmHeader()
		, m_mmrmContext()
		, m_localTM(Matrix34::CreateIdentity())
		, m_State(CREATED)
		, m_needsBaking(true)
	{}

	~SDeformableData() {}
};

CDeformableNode::CDeformableNode()
	: m_pData()
	, m_nData()
	, m_wind()
	, m_nFrameId()
	, m_Colliders()
	, m_nColliders()
	, m_Projectiles()
	, m_nProjectiles()
	, m_pHeap()
	, m_numIndices()
	, m_numVertices()
	, m_renderMesh()
	, m_pStatObj()
	, m_all_prepared()
{
	m_bbox = AABB(Vec3(0, 0, 0));
}

CDeformableNode::~CDeformableNode()
{
	ClearSimulationData();
	ClearInstanceData();
}

void CDeformableNode::ClearInstanceData()
{
	gEnv->pJobManager->WaitForJob(m_cullState);
	gEnv->pJobManager->WaitForJob(m_updateState);
	for (size_t i = 0; i < m_nData; ++i)
	{
		gEnv->pJobManager->WaitForJob(m_pData[i]->m_mmrmHeader.procGeom->geomPrepareState);
		if (m_pHeap && m_pHeap->UsableSize(m_pData[i]->m_mmrmHeader.deform_vertices))
		{
			m_pHeap->Free(m_pData[i]->m_mmrmHeader.deform_vertices);
			m_pData[i]->m_mmrmHeader.deform_vertices = NULL;
		}
		else if (m_pData[i]->m_mmrmHeader.deform_vertices)
		{
			CryModuleMemalignFree(m_pData[i]->m_mmrmHeader.deform_vertices);
			m_pData[i]->m_mmrmHeader.deform_vertices = NULL;
		}
		delete m_pData[i];
	}
	if (m_pData) { CryModuleMemalignFree(m_pData); m_pData = NULL; }
	if (m_Colliders) { CryModuleMemalignFree(m_Colliders); m_Colliders = NULL; }
	if (m_Projectiles) { CryModuleMemalignFree(m_Projectiles); m_Projectiles = NULL; }
	if (m_wind) { CryModuleMemalignFree(m_wind); m_wind = NULL; }
	m_bbox.Reset();
	stl::free_container(m_renderChunks);
	m_numIndices = m_numVertices = 0;
	m_renderMesh = NULL;
	m_nData = 0u;
	m_nFrameId = 0u;
	m_nColliders = 0u;
	m_pHeap = NULL;
	m_all_prepared = false;
}

void CDeformableNode::ClearSimulationData()
{
	for (size_t i = 0; i < m_nData; ++i)
	{
		gEnv->pJobManager->WaitForJob(m_pData[i]->m_mmrmHeader.procGeom->geomPrepareState);
		if (m_pHeap && m_pHeap->UsableSize(m_pData[i]->m_mmrmHeader.deform_vertices))
		{
			m_pHeap->Free(m_pData[i]->m_mmrmHeader.deform_vertices);
			m_pData[i]->m_mmrmHeader.deform_vertices = NULL;
		}
		else if (m_pData[i]->m_mmrmHeader.deform_vertices)
		{
			CryModuleMemalignFree(m_pData[i]->m_mmrmHeader.deform_vertices);
			m_pData[i]->m_mmrmHeader.deform_vertices = NULL;
		}
		m_pData[i]->m_State = SDeformableData::PREPARED;
	}
	m_bbox.Reset();
	stl::free_container(m_renderChunks);
	m_numIndices = m_numVertices = 0;
	m_renderMesh = NULL;
	m_pHeap = NULL;
	m_all_prepared = false;
}

void CDeformableNode::CreateInstanceData(SDeformableData* pData, CStatObj* pStatObj)
{
	SMMRMGroupHeader* header = &pData->m_mmrmHeader;
	header->procGeom = s_GeomManager.GetGeometry(pStatObj);
	header->physConfig.Update(header->procGeom);
	header->instGroupId = 0;
	header->maxViewDistance = 0;
	header->lodRationNorm = 0;

	SMMRMUpdateContext* update = &pData->m_mmrmContext;
	update->group = header;

	resize_list(header->instances, header->numSamplesAlloc = header->numSamples = 1, 128);

	header->instances[0].pos_x = 0u;
	header->instances[0].pos_y = 0u;
	header->instances[0].pos_z = 0u;
	CompressQuat(Quat::CreateIdentity(), header->instances[0]);
	header->instances[0].scale = (uint8)SATURATEB(1.f * VEGETATION_CONV_FACTOR);
	header->instances[0].lastLod = 0;

	pData->m_State = SDeformableData::PREPARED;
}

void CDeformableNode::SetStatObj(CStatObj* pStatObj)
{
	ClearInstanceData();

	if (pStatObj == NULL || Cry3DEngineBase::GetCVars()->e_MergedMeshes == 0)
		return;

	// Create deformable subobject is present
	if (pStatObj->m_isDeformable)
	{
		resize_list(m_pData, m_nData = 1, 16);
		CreateInstanceData(m_pData[0] = new SDeformableData, pStatObj);
	}
	else
		for (int i = 0; i < pStatObj->GetSubObjectCount(); ++i)
		{
			IStatObj::SSubObject* subObject = pStatObj->GetSubObject(i);
			if (!subObject)
				continue;
			if (CStatObj* pChild = static_cast<CStatObj*>(pStatObj->GetSubObject(i)->pStatObj))
			{
				if (!pChild->m_isDeformable)
					continue;
				resize_list(m_pData, m_nData + 1, 16);
				m_pData[m_nData] = new SDeformableData;
				if (!subObject->bIdentityMatrix)
					m_pData[m_nData]->m_localTM = subObject->tm;
				CreateInstanceData(m_pData[m_nData++], pChild);
			}
		}

	m_pStatObj = pStatObj;
}

void CDeformableNode::CreateDeformableSubObject(bool create, const Matrix34& worldTM, IGeneralMemoryHeap* pHeap)
{
	ClearSimulationData();

	if (create == false || Cry3DEngineBase::GetCVars()->e_MergedMeshes == 0)
		return;

	m_pHeap = pHeap;

	BakeDeform(worldTM);
}

void CDeformableNode::UpdateInternalDeform(
  SDeformableData* pData, CRenderObject* pRenderObject, const AABB& bbox
  , const SRenderingPassInfo& passInfo
  , _smart_ptr<IRenderMesh>& rm
  , strided_pointer<SVF_P3S_C4B_T2S> vtxBuf
  , strided_pointer<SPipTangents> tgtBuf
  , vtx_idx* idxBuf
  , size_t& iv
  , size_t& ii)
{
	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);
	SMMRMGroupHeader* group = &pData->m_mmrmHeader;
	if (group == NULL)
		return;
	SMMRMGeometry* geom = group->procGeom;
	if (!geom)
		return;
	SMMRMUpdateContext* update = &pData->m_mmrmContext;
	SMMRMGroupHeader* header = &pData->m_mmrmHeader;
	if (pData->m_State == SDeformableData::READY)
	{
		FUNCTION_PROFILER_3DENGINE;
		Matrix34 worldTM = pRenderObject->GetMatrix(passInfo);

		// Create a new render mesh and dispatch the asynchronous updates
		size_t indices = group->procGeom->numIdx, vertices = group->procGeom->numVtx;

		update->general = (SVF_P3S_C4B_T2S*)vtxBuf.data;
		update->tangents = (SPipTangents*)tgtBuf.data;
		update->idxBuf = idxBuf;
		update->updateFlag = rm->SetAsyncUpdateState();
		update->colliders = m_Colliders;
		update->ncolliders = m_nColliders;
		update->projectiles = m_Projectiles;
		update->nprojectiles = m_nProjectiles;
		update->max_iter = 2;
		update->dt = gEnv->pTimer->GetFrameTime();
		update->abstime = gEnv->pTimer->GetCurrTime();
		update->_min = bbox.min;
		update->_max = bbox.max;
		update->wind = m_wind;
		update->use_spines = 0;
		update->chunks.resize(group->numVisbleChunks = group->procGeom->numChunks[0]);
		update->frame_count = passInfo.GetMainFrameID();
		for (size_t j = 0; j < group->procGeom->numChunks[0]; ++j)
		{
			update->chunks[j].ioff = ii;
			update->chunks[j].voff = iv;
			update->chunks[j].matId = group->procGeom->pChunks[0][j].matId;
			ii += (update->chunks[j].icnt = group->procGeom->pChunks[0][j].nindices);
			iv += (update->chunks[j].vcnt = group->procGeom->pChunks[0][j].nvertices);
		}
#if MMRM_USE_BOUNDS_CHECK
		for (size_t u = 0; u < update->chunks.size(); ++u)
		{
			if (((SVF_P3S_C4B_T2S*)vtxBuf.data) + update->chunks[u].voff >= (((SVF_P3S_C4B_T2S*)vtxBuf.data) + vertices)) __debugbreak();
			if (((SVF_P3S_C4B_T2S*)vtxBuf.data) + update->chunks[u].voff + (update->chunks[u].vcnt - 1) >= (((SVF_P3S_C4B_T2S*)vtxBuf.data) + vertices)) __debugbreak();
		}
		update->general_end = ((SVF_P3S_C4B_T2S*)vtxBuf.data) + vertices;
		update->tangents_end = ((SPipTangents*)tgtBuf.data) + vertices;
		update->idx_end = idxBuf + indices;
#endif

		if (group->deform_vertices)
		{
			if (pData->m_needsBaking)
				BakeInternal(pData, worldTM);

#if MMRM_USE_JOB_SYSTEM
			TPVRNUpdateRenderMeshDeform job(&s_mmrm_globals.camera, 0u);
			job.RegisterJobState(&m_updateState);
			job.SetClassInstance(update);
			job.SetPriorityLevel(JobManager::eLowPriority);
			job.Run();
#else
			update->MergeInstanceMeshesDeform(&s_mmrm_globals.camera, 0u);
#endif
		}
	}
}

void CDeformableNode::RenderInternalDeform(
  CRenderObject* pRenderObject, int nLod, const AABB& bbox
  , const SRenderingPassInfo& passInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);
	if (nLod > 0 || m_nData == 0 || Cry3DEngineBase::GetCVars()->e_MergedMeshes == 0)
		return;

	float radius = max(bbox.max.x - bbox.min.x, max(bbox.max.y - bbox.min.y, bbox.max.z - bbox.min.z)) * 0.5f;
	Vec3 centre = (bbox.max + bbox.min) * 0.5f;
	AABB radbox(centre, radius);

	if (passInfo.IsGeneralPass() && ((passInfo.GetCamera().GetPosition() - bbox.GetCenter()).len2() < sqr(((IRenderNode*)pRenderObject->m_pRenderNode)->GetMaxViewDist()) * Cry3DEngineBase::GetCVars()->e_MergedMeshesDeformViewDistMod))
	{
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "CDeformableNode::RenderInternalDeform JobSync");
			gEnv->pJobManager->WaitForJob(m_cullState);
			gEnv->pJobManager->WaitForJob(m_updateState);
		}

		m_nFrameId = passInfo.GetMainFrameID();

		if (m_bbox.IsReset() == false)
		{
			Matrix34 worldTM = pRenderObject->GetMatrix(passInfo);
			AABB cbbox = AABB::CreateTransformedAABB(worldTM, m_bbox);
			QueryColliders(m_Colliders, m_nColliders, cbbox);
			QueryProjectiles(m_Projectiles, m_nProjectiles, cbbox);
		}
		SampleWind(m_wind, bbox);

#if MMRM_RENDER_DEBUG && MMRM_VISUALIZE_WINDFIELD
		VisualizeWindField(m_wind, radbox, true, true);
#endif

		bool update_chunks = m_all_prepared && !m_renderChunks.size(), all_prepared = true;
		for (size_t i = 0; i < m_nData; CryPrefetch(m_pData[i++]))
			;
		for (size_t i = 0; i < m_nData; ++i)
		{
			SDeformableData* pData = m_pData[i];
			SMMRMGroupHeader* group = &pData->m_mmrmHeader;
			SMMRMGeometry* geom = group->procGeom;
			if (!geom)
				return;
			switch (pData->m_State)
			{
			case SDeformableData::PREPARED:
				if (geom->state != SMMRMGeometry::PREPARED)
				{
					all_prepared = false;
					break; // return or else problems with the preparing below
				}
				if (m_pHeap)
					group->deform_vertices = (SMMRMDeformVertex*)m_pHeap->Memalign(16, sizeof(SMMRMDeformVertex) * group->numSamples * group->procGeom->deform->nvertices, "deformable vertices");
				if (!group->deform_vertices)
					resize_list(group->deform_vertices, group->numSamples * group->procGeom->deform->nvertices, 16);
				memset(group->deform_vertices, 0x0, sizeof(SMMRMDeformVertex) * group->numSamples * group->procGeom->deform->nvertices);
				for (size_t k = 0; group->procGeom->deform && k < group->procGeom->deform->nvertices; ++k)
				{
					group->deform_vertices[k].pos[0] = group->deform_vertices[k].pos[1] = group->procGeom->deform->initial[k];
					group->deform_vertices[k].vel = Vec3(0, 0, 0);
				}
				m_bbox.Add(group->procGeom->aabb.min);
				m_bbox.Add(group->procGeom->aabb.max);
				pData->m_State = SDeformableData::READY;
			case SDeformableData::READY: // fallthru from PREPARED
				IF (update_chunks, 0)
				{
					for (size_t j = 0; j < geom->numChunks[0]; ++j)
					{
						CRenderChunk chunk;
						chunk.m_nMatID = geom->pChunks[0][j].matId;
						chunk.nFirstIndexId = m_numIndices;
						chunk.nFirstVertId = m_numVertices;
						chunk.nNumIndices = geom->pChunks[0][j].nindices;
						chunk.nNumVerts = geom->pChunks[0][j].nvertices;
						m_renderChunks.push_back(chunk);

						m_numVertices += geom->pChunks[0][j].nvertices;
						m_numIndices += geom->pChunks[0][j].nindices;
					}
				}
				break;
			default:
				all_prepared = false;
				break;
			}
		}
		if (!(m_all_prepared = all_prepared) || m_numVertices + m_numIndices == 0)
			return;

		m_renderMesh = gEnv->pRenderer->CreateRenderMeshInitialized(
		  NULL, m_numVertices, EDefaultInputLayouts::P3S_C4B_T2S, NULL, m_numIndices,
		  prtTriangleList, "MergedMesh", "MergedMesh", eRMT_Dynamic);

		m_renderMesh->LockForThreadAccess();
		strided_pointer<SPipTangents> tgtBuf;
		strided_pointer<SVF_P3S_C4B_T2S> vtxBuf;
		vtx_idx* idxBuf = NULL;

		vtxBuf.data = (SVF_P3S_C4B_T2S*)m_renderMesh->GetPosPtrNoCache(vtxBuf.iStride, FSL_CREATE_MODE);
		tgtBuf.data = (SPipTangents*)m_renderMesh->GetTangentPtr(tgtBuf.iStride, FSL_CREATE_MODE);
		idxBuf = m_renderMesh->GetIndexPtr(FSL_CREATE_MODE);

		if (!vtxBuf || !idxBuf || !tgtBuf)
		{
			m_renderMesh->UnlockStream(VSF_GENERAL);
			m_renderMesh->UnlockStream(VSF_TANGENTS);
			m_renderMesh->UnlockIndexStream();
			m_renderMesh->UnLockForThreadAccess();
			return;
		}

		for (size_t i = 0, iv = 0, ii = 0; i < m_nData; ++i)
			UpdateInternalDeform(m_pData[i], pRenderObject, radbox, passInfo,
			                     m_renderMesh, vtxBuf, tgtBuf, idxBuf, iv, ii);

		m_renderMesh->SetRenderChunks(&m_renderChunks[0], m_renderChunks.size(), false);
		m_renderMesh->UnLockForThreadAccess();
	}

	if (!m_renderMesh)
		return;
	CRenderObject* ro = gEnv->pRenderer->EF_DuplicateRO(pRenderObject, passInfo);
	if (!ro)
		return;
	ro->m_ObjFlags |= FOB_DYNAMIC_OBJECT;
	ro->SetMatrix(Matrix34::CreateTranslationMat(centre), passInfo);
	m_renderMesh->Render(ro, passInfo);
}

void CDeformableNode::BakeDeform(const Matrix34& worldTM)
{
	for (size_t i = 0; i < m_nData; ++i)
	{
		SDeformableData* pData = m_pData[i];
		pData->m_needsBaking = true;
		if (pData->m_State != SDeformableData::READY)
			continue;
		BakeInternal(pData, worldTM);
	}
}

void CDeformableNode::BakeInternal(SDeformableData* pData, const Matrix34& tm)
{
	SMMRMGroupHeader* group = &pData->m_mmrmHeader;
	if (group == NULL)
		return;
	SMMRMGeometry* geom = group->procGeom;
	if (!geom)
		return;

	Matrix34 worldTM;
	worldTM = tm * pData->m_localTM;

	SMMRMUpdateContext* update = &pData->m_mmrmContext;
	SMMRMDeformVertex* deform_vertices = update->group->deform_vertices;
	for (size_t i = 0; i < geom->deform->nvertices; ++i)
	{
		const Vec3 npos = (worldTM * geom->deform->initial[i]);
		deform_vertices[i].pos[0] = deform_vertices[i].pos[1] = npos;
		deform_vertices[i].vel = Vec3(0, 0, 0);
	}
	pData->m_needsBaking = false;
}

//////////////////////////////////////////////////////////////////////
// Segmented World

void CMergedMeshRenderNode::OffsetPosition(const Vec3& delta)
{
	m_internalAABB.Move(delta);
	m_visibleAABB.Move(delta);
	m_pos += delta;
}

void CMergedMeshRenderNode::FillBBox(AABB& aabb)
{
	aabb = CMergedMeshRenderNode::GetBBox();
}

EERType CMergedMeshRenderNode::GetRenderNodeType()
{
	return eERType_MergedMesh;
}

float CMergedMeshRenderNode::GetMaxViewDist()
{
	float radius = m_internalAABB.GetRadius();
	return max(GetCVars()->e_ViewDistMin, radius * GetCVars()->e_MergedMeshesViewDistRatio);
}

Vec3 CMergedMeshRenderNode::GetPos(bool bWorldOnly) const
{
	return m_pos;
}

IMaterial* CMergedMeshRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return NULL;
}

#undef FSL_CREATE_MODE
