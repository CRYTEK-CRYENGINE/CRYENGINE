// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef AIQUADTREE_H
#define AIQUADTREE_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <vector>

// The element stored in CAIQuadTree must provide the following interface:
//
// The z component of the AABB should be ignored:
// bool DoesIntersectAABB(const AABB& aabb);
//
// const AABB &GetAABB();
//
// There should be a counter that CAIQuadTree can use internally - shouldn't
// be used for anything else:
// mutable int m_counter
//
// Returns some debug info about this element
// const char *GetDebugName() const

template<typename SAIQuadTreeElement>
class CAIQuadTree
{
public:
	CAIQuadTree();
	~CAIQuadTree();

	/// Adds a element to the list of elements, but doesn't add it to the QuadTree
	void AddElement(const SAIQuadTreeElement& element);

	/// Clears elements and cells
	/// if the freeMemory is set to true, the element index array will be freed,
	/// otherwise it will be reset preserving the allocated memory.
	void Clear(bool freeMemory = true);

	/// Builds the QuadTree from scratch (not incrementally) - deleting any previous
	/// tree.
	/// Building the QuadTree will involve placing all elements into the root cell.
	/// Then this cell gets pushed onto a stack of cells to examine. This stack
	/// will get parsed and every cell containing more than maxElementsPerCell
	/// will get split into 8 children, and all the original elements in that cell
	/// will get partitioned between the children. A element can end up in multiple
	/// cells (possibly a lot!) if it straddles a boundary. Therefore when intersection
	/// tests are done SAIQuadTreeElement::m_counter can be set/tested using a counter to avoid
	/// properly testing the triangle multiple times (the counter _might_ wrap around,
	/// so when it wraps ALL the element flags should be cleared! Could do this
	/// incrementally...).
	void BuildQuadTree(int maxElementsPerCell, float minCellSize);

	/// Returns elements that claim to intersect the point (ignoring z), and the number.
	unsigned GetElements(std::vector<const SAIQuadTreeElement*>& elements, const Vec3& point) const;

	/// Returns elements that claim to intersect the aabb (ignoring z), and the number.
	unsigned GetElements(std::vector<const SAIQuadTreeElement*>& elements, const AABB& aabb) const;

	/// Dumps our contents
	void Dump(const char* debugName) const;

private:
	/// Internally we don't store pointers but store indices into a single contiguous
	/// array of cells and triangles (so that the vectors can get resized).
	///
	/// Each cell will either contain children OR contain triangles.
	struct CQuadTreeCell
	{
		/// constructor clears everything
		CQuadTreeCell();
		/// constructor clears everything
		CQuadTreeCell(const AABB& aabb);
		/// Sets all child indices to -1 and clears the triangle indices.
		void Clear();

		/// Indicates if we contain triangles (if not then we should/might have children)
		bool IsLeaf() const { return m_childCellIndices[0] == -1; }

		/// indices into the children - P means "plus" and M means "minus" and the
		/// letters are xy. So PM means +ve x, -ve y
		enum EChild
		{
			PP,
			PM,
			MP,
			MM,
			NUM_CHILDREN
		};

		/// indices of the children (if not leaf). Will be -1 if there is no child
		int m_childCellIndices[NUM_CHILDREN];

		/// indices of the elements (if leaf)
		std::vector<int> m_elementIndices;

		/// Bounding box for the space we own
		AABB m_aabb;
	};

	/// Functor that can be passed to std::sort so that it sorts equal sized cells along a specified
	/// direction such that cells near the beginning of a line with dirPtr come at the end of the
	/// sorted container. This means they get processed first when that container is used as a stack.
	struct CCellSorter
	{
		CCellSorter(const Vec3* dirPtr, const std::vector<CQuadTreeCell>* cellsPtr) : m_dirPtr(dirPtr), m_cellsPtr(cellsPtr) {}
		bool operator()(int cell1Index, int cell2Index) const
		{
			Vec3 delta = (*m_cellsPtr)[cell2Index].m_aabb.min - (*m_cellsPtr)[cell1Index].m_aabb.min;
			return (delta * *m_dirPtr) < 0.0f;
		}
		const Vec3*                       m_dirPtr;
		const std::vector<CQuadTreeCell>* m_cellsPtr;
	};

	/// Create a bounding box appropriate for a child, based on a parents AABB
	AABB CreateAABB(const AABB& aabb, typename CQuadTreeCell::EChild child) const;

	/// Returns true if the triangle intersects or is contained by a cell
	bool DoesElementIntersectCell(const SAIQuadTreeElement& element, const CQuadTreeCell& cell) const;

	/// Increment our test counter, wrapping around if necessary and zapping the
	/// triangle counters.
	/// Const because we only modify mutable members.
	void IncrementTestCounter() const;

	/// Dumps the cell and all its children, indented
	void DumpCell(const CQuadTreeCell& cell, int indentLevel) const;

	/// All our cells. The only thing guaranteed about this is that m_cell[0] (if
	/// it exists) is the root cell.
	std::vector<CQuadTreeCell>      m_cells;
	/// All our elements.
	std::vector<SAIQuadTreeElement> m_elements;

	AABB                            m_boundingBox;

	/// During intersection testing we keep a stack of cells to test (rather than recursing) -
	/// to avoid excessive memory allocation we don't free the memory between calls unless
	/// the user calls FreeTemporaryMemory();
	mutable std::vector<int> m_cellsToTest;

	/// Counter used to prevent multiple tests when triangles are contained in more than
	/// one cell
	mutable unsigned m_testCounter;
};

#include "AIQuadTree.inl"

#endif
