// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   WorldOctree.h
   Version:     v1.00
   Description:

   -------------------------------------------------------------------------
   History:
   - 23:2:2005   Created by Danny Chapman
   - 2 Mar 2009			 : Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/
#ifndef WORLDOCTREE_H
#define WORLDOCTREE_H

#if _MSC_VER > 1000
	#pragma once
#endif

/// Stores world collision data in an octree structure for quick ray testing
class CWorldOctree
{
public:
	CWorldOctree();
	~CWorldOctree();

	struct CTriangle
	{
		CTriangle() {};
		CTriangle(const Vec3& v0, const Vec3& v1, const Vec3& v2) :
			m_v0(v0), m_v1(v1), m_v2(v2), m_counter(-1) {}
		Vec3 m_v0, m_v1, m_v2;
		/// marker - this will get set to the test number after this triangle has been
		/// checked in an intersection test so that it doesn't need to be checked more
		/// than once.
		mutable int m_counter;
	};

	/// Adds a triangle to the list of triangles, but doesn't add it to the octree
	void AddTriangle(const CTriangle& triangle);

	/// Clears triangles and cells
	/// if the freeMemory is set to true, the triangle index array will be freed,
	/// otherwise it will be reset preserving the allocated memory.
	void Clear(bool freeMemory = true);

	/// Builds the octree from scratch (not incrementally) - deleting any previous
	/// tree.
	/// Building the octree will involve placing all triangles into the root cell.
	/// Then this cell gets pushed onto a stack of cells to examine. This stack
	/// will get parsed and every cell containing more than maxTrianglesPerCell
	/// will get split into 8 children, and all the original triangles in that cell
	/// will get partitioned between the children. A triangle can end up in multiple
	/// cells (possibly a lot!) if it straddles a boundary. Therefore when intersection
	/// tests are done CTriangle::m_counter can be set/tested using a counter to avoid
	/// properly testing the triangle multiple times (the counter _might_ wrap around,
	/// so when it wraps ALL the triangle flags should be cleared! Could do this
	/// incrementally...).
	void BuildOctree(int maxTrianglesPerCell, float minCellSize, const AABB& bbox);

	/// Simple overlap test of capsule with the triangles
	bool OverlapCapsule(const Lineseg& lineseg, float radius);

	struct CIntersectionData
	{
		float t;
		Vec3  pt, normal;
	};
	/// Intersect the line segment with the triangles - returns true if there is an intersection
	/// and if so if data is non-zero it returns the data associated with the first
	/// intersection. If data is zero it returns at the first intersection found (so
	/// can be faster).
	/// if twoSided = true then the test is two-sided (i.e. return hits with back-facing
	/// triangles, in which case the normal is still the triangle normal)
	/// startingCell indicates the cell to start the search from - should have been returned
	/// from FindCellContainingAABB.
	bool IntersectLineseg(CIntersectionData* data, Lineseg lineseg, bool twoSided, int startingCell = 0) const;

	/// As the other IntersectLineseg but this uses a list of cells that has previously been
	/// obtained using GetCellsOverlappingAABB
	bool IntersectLinesegFromCache(CIntersectionData* data, Lineseg lineseg, bool twoSided) const;

	/// As the other IntersectLineseg but this uses a list of cells that has previously been
	/// obtained using GetCellsOverlappingAABB2
	bool IntersectLinesegFromCache2(CIntersectionData* data, Lineseg lineseg, bool twoSided) const;

	/// Cache a list of all leaf cells overlapping aabb - these can be used by
	/// IntersectLinesegFromCache for quick subsequent checks.
	void CacheCellsOverlappingAABB(const AABB& aabb);

	/// Cache a list of all leaf cells overlapping sphere - these can be used by
	/// IntersectLinesegFromCache for quick subsequent checks.
	void CacheCellsOverlappingSphere(const Vec3& pos, float radius);

	/// Cache a list of all cells that are either (1) leaf cells that intersect or are contained
	/// by the boundary of the aabb or (2) non-leaf cells that are completely within the aabb.
	/// these can be used by IntersectLinesegFromCache2 for quick subsequent checks.
	void CacheCellsOverlappingAABB2(const AABB& aabb);

	/// Cache a list of all cells that are either (1) leaf cells that intersect or are contained
	/// by the boundary of the sphere or (2) non-leaf cells that are completely within the sphere.
	/// these can be used by IntersectLinesegFromCache2 for quick subsequent checks.
	void CacheCellsOverlappingSphere2(const Vec3& pos, float radius);

	/// Returns the smallest cell containg the aabb, for subsequent passing into IntersectLineseg
	/// Only makes sense if the aabb is pretty small compared to the domain so that subsequent
	/// tests can avoid a lot of cell tests.
	int FindCellContainingAABB(const AABB& aabb) const;

	/// Intersection tests allocate memory but keep it from one test to the next - call
	/// this to free this temporary memory.
	void FreeTemporaryMemory() { ClearVectorMemory(m_cellsToTest); ClearVectorMemory(m_cachedCells); ClearVectorMemory(m_cachedCells2); }

	/// Dump some statistics and validate
	void DumpStats() const;

	/// draw the cells/triangles etc
	void DebugDraw() const;

	/// Get memory usage in bytes
	size_t MemStats();

private:
	/// Internally we don't store pointers but store indices into a single contiguous
	/// array of cells and triangles owned by CWorldOctree (so that the vectors can
	/// get resized).
	///
	/// Each cell will either contain children OR contain triangles.
	struct COctreeCell
	{
		/// constructor clears everything
		COctreeCell();
		/// constructor clears everything
		COctreeCell(const AABB& aabb);
		/// Sets all child indices to -1 and clears the triangle indices.
		void Clear();

		/// Indicates if we contain triangles (if not then we should/might have children)
		bool IsLeaf() const { return m_childCellIndices[0] == -1; }

		/// indices into the children - P means "plus" and M means "minus" and the
		/// letters are xyz. So PPM means +ve x, +ve y, -ve z
		enum EChild
		{
			PPP,
			PPM,
			PMP,
			PMM,
			MPP,
			MPM,
			MMP,
			MMM,
			NUM_CHILDREN
		};

		/// indices of the children (if not leaf). Will be -1 if there is no child
		int m_childCellIndices[NUM_CHILDREN];

		/// indices of the triangles (if leaf)
		std::vector<int> m_triangleIndices;

		/// Bounding box for the space we own
		AABB m_aabb;
	};

	/// Functor that can be passed to std::sort so that it sorts equal sized cells along a specified
	/// direction such that cells near the beginning of a line with dirPtr come at the end of the
	/// sorted container. This means they get processed first when that container is used as a stack.
	struct CCellSorter
	{
		CCellSorter(const Vec3* dirPtr, const std::vector<COctreeCell>* cellsPtr) : m_dirPtr(dirPtr), m_cellsPtr(cellsPtr) {}
		bool operator()(int cell1Index, int cell2Index) const
		{
			Vec3 delta = (*m_cellsPtr)[cell2Index].m_aabb.min - (*m_cellsPtr)[cell1Index].m_aabb.min;
			return (delta * *m_dirPtr) < 0.0f;
		}
		const Vec3*                     m_dirPtr;
		const std::vector<COctreeCell>* m_cellsPtr;
	};

	/// Create a bounding box appropriate for a child, based on a parents AABB
	AABB CreateAABB(const AABB& aabb, COctreeCell::EChild child) const;

	/// Returns true if the triangle intersects or is contained by a cell
	bool DoesTriangleIntersectCell(const CTriangle& triangle, const COctreeCell& cell) const;

	/// Increment our test counter, wrapping around if necessary and zapping the
	/// triangle counters.
	/// Const because we only modify mutable members.
	void IncrementTestCounter() const;

	/// All our cells. The only thing guaranteed about this is that m_cell[0] (if
	/// it exists) is the root cell.
	std::vector<COctreeCell> m_cells;
	/// All our triangles.
	std::vector<CTriangle>   m_triangles;

	AABB                     m_boundingBox;

	/// During intersection testing we keep a stack of cells to test (rather than recursing) -
	/// to avoid excessive memory allocation we don't free the memory between calls unless
	/// the user calls FreeTemporaryMemory();
	mutable std::vector<int> m_cellsToTest;

	/// cell indices cached from CacheCellsOverlappingAABB
	std::vector<int> m_cachedCells;

	/// cell indices cached from CacheCellsOverlappingAABB2
	/// This is mutable because during the intersection test we use it as a stack
	/// but restore it at the end.
	mutable std::vector<int> m_cachedCells2;

	/// Counter used to prevent multiple tests when triangles are contained in more than
	/// one cell
	mutable unsigned m_testCounter;
};

#endif
