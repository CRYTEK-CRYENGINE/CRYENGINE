// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//====================================================================
// Clear
//====================================================================
template<typename SAIQuadTreeElement>
void CAIQuadTree<SAIQuadTreeElement>::CQuadTreeCell::Clear()
{
	for (unsigned i = 0; i < NUM_CHILDREN; ++i)
		m_childCellIndices[i] = -1;
	m_elementIndices.clear();
}

//====================================================================
// CQuadTreeCell
//====================================================================
template<typename SAIQuadTreeElement>
CAIQuadTree<SAIQuadTreeElement>::CQuadTreeCell::CQuadTreeCell()
{
	Clear();
}

//====================================================================
// ~CQuadTreeCell
//====================================================================
template<typename SAIQuadTreeElement>
CAIQuadTree<SAIQuadTreeElement>::CQuadTreeCell::CQuadTreeCell(const AABB& aabb)
	:
	m_aabb(aabb)
{
	Clear();
}

//============================ CAIQuadTree ==========================

//===================================================================
// CAIQuadTree
//===================================================================
template<typename SAIQuadTreeElement>
CAIQuadTree<SAIQuadTreeElement>::CAIQuadTree()
	:
	m_testCounter(0)
{
}

//===================================================================
// ~CAIQuadTree
//===================================================================
template<typename SAIQuadTreeElement>
CAIQuadTree<SAIQuadTreeElement>::~CAIQuadTree()
{
}

//===================================================================
// AddElement
//===================================================================
template<typename SAIQuadTreeElement>
void CAIQuadTree<SAIQuadTreeElement >::AddElement(const SAIQuadTreeElement& element)
{
	m_elements.push_back(element);
}

//===================================================================
// Clear
//===================================================================
template<typename SAIQuadTreeElement>
void CAIQuadTree<SAIQuadTreeElement >::Clear(bool freeMemory)
{
	if (freeMemory)
	{
		m_cells.clear();
		m_elements.clear();
	}
	else
	{
		m_cells.resize(0);
		m_elements.resize(0);
	}
}

//====================================================================
// IncrementTestCounter
//====================================================================
template<typename SAIQuadTreeElement>
inline void CAIQuadTree<SAIQuadTreeElement >::IncrementTestCounter() const
{
	++m_testCounter;
	if (m_testCounter == 0)
	{
		// wrap around - clear all the element counters
		const unsigned numElements = m_elements.size();
		for (unsigned i = 0; i != numElements; ++i)
			m_elements[i].m_counter = 0;
		m_testCounter = 1;
	}
}

//====================================================================
// DoesElementIntersectCell
//====================================================================
template<typename SAIQuadTreeElement>
bool CAIQuadTree<SAIQuadTreeElement >::DoesElementIntersectCell(const SAIQuadTreeElement& element,
                                                                const CQuadTreeCell& cell) const
{
	return element.DoesIntersectAABB(cell.m_aabb);
}

//====================================================================
// CreateAABB
//====================================================================
template<typename SAIQuadTreeElement>
AABB CAIQuadTree<SAIQuadTreeElement >::CreateAABB(const AABB& aabb, typename CQuadTreeCell::EChild child) const
{
	Vec3 dims = 0.5f * (aabb.max - aabb.min);
	Vec3 offset;
	switch (child)
	{
	case CQuadTreeCell::PP:
		offset.Set(1, 1, 0);
		break;
	case CQuadTreeCell::PM:
		offset.Set(1, 0, 0);
		break;
	case CQuadTreeCell::MP:
		offset.Set(0, 1, 0);
		break;
	case CQuadTreeCell::MM:
		offset.Set(0, 0, 0);
		break;
	default:
		AIWarning("CAIQuadTree::CreateAABB Got impossible child: %d", child);
		offset.Set(0, 0, 0);
		break;
	}

	AABB result;
	result.min = aabb.min + Vec3(offset.x * dims.x, offset.y * dims.y, 0.0f);
	result.max = result.min + dims;
	// expand it just a tiny bit just to be safe!
	float extra = 0.00001f;
	result.min -= extra * dims;
	result.max += extra * dims;
	return result;
}

//===================================================================
// ElementOutsideAABB
//===================================================================
struct ElementOutsideAABB
{
	ElementOutsideAABB(const AABB& aabb) : m_aabb(aabb) {}

	template<typename SAIQuadTreeElement>
	bool operator()(const SAIQuadTreeElement& element)
	{
		return !element.DoesIntersectAABB(m_aabb);
	}
	const AABB m_aabb;
};

//====================================================================
// BuildOctree
//====================================================================
template<typename SAIQuadTreeElement>
void CAIQuadTree<SAIQuadTreeElement >::BuildQuadTree(int maxElementsPerCell, float minCellSize)
{
	m_boundingBox.Reset();
	for (typename std::vector<SAIQuadTreeElement>::iterator it = m_elements.begin(); it != m_elements.end(); ++it)
	{
		m_boundingBox.Add(it->GetAABB());
		it->m_counter = -1;
	}

	// clear any existing cells
	m_cells.clear();
	m_testCounter = 0;

	// set up the root
	m_cells.push_back(CQuadTreeCell(m_boundingBox));
	unsigned numElements = m_elements.size();
	m_cells.back().m_elementIndices.resize(numElements);
	for (unsigned i = 0; i < numElements; ++i)
		m_cells.back().m_elementIndices[i] = i;

	// rather than doing things recursively, use a stack of cells that need
	// to be processed - for each cell if it contains too many elements we
	// create child cells and move the elements down into them (then we
	// clear the parent elements).
	std::vector<int> cellsToProcess;
	cellsToProcess.push_back(0);

	// bear in mind during this that any time a new cell gets created any pointer
	// or reference to an existing cell may get invalidated - so use indexing.
	while (!cellsToProcess.empty())
	{
		int cellIndex = cellsToProcess.back();
		cellsToProcess.pop_back();

		if ((int)m_cells[cellIndex].m_elementIndices.size() <= maxElementsPerCell)
			continue;
		float radius = m_cells[cellIndex].m_aabb.GetSize().GetLength2D() * 0.5f;
		if (radius < minCellSize)
			continue;

		// we need to put these elements into the children
#ifdef _DEBUG
		std::set<int> handledElements;
#endif
		for (unsigned iChild = 0; iChild < CQuadTreeCell::NUM_CHILDREN; ++iChild)
		{
			m_cells[cellIndex].m_childCellIndices[iChild] = (int) m_cells.size();
			cellsToProcess.push_back((int) m_cells.size());
			m_cells.push_back(CQuadTreeCell(CreateAABB(m_cells[cellIndex].m_aabb, (typename CQuadTreeCell::EChild)iChild)));

			CQuadTreeCell& childCell = m_cells.back();
			unsigned numCellElements = m_cells[cellIndex].m_elementIndices.size();
			for (unsigned i = 0; i < numCellElements; ++i)
			{
				int iElement = m_cells[cellIndex].m_elementIndices[i];
				const SAIQuadTreeElement& element = m_elements[iElement];
				if (DoesElementIntersectCell(element, childCell))
				{
					childCell.m_elementIndices.push_back(iElement);
#ifdef _DEBUG
					handledElements.insert(iElement);
#endif
				}
			}
		}
#ifdef _DEBUG
		AIAssert(handledElements.size() == m_cells[cellIndex].m_elementIndices.size());
#endif
		// the children handle all the elements now - we no longer need them
		m_cells[cellIndex].m_elementIndices.clear();
	}
}

//===================================================================
// GetElements
//===================================================================
template<typename SAIQuadTreeElement>
unsigned CAIQuadTree<SAIQuadTreeElement >::GetElements(std::vector<const SAIQuadTreeElement*>& elements, const Vec3& point) const
{
	elements.resize(0);

	if (m_cells.empty())
		return 0;

	m_cellsToTest.resize(0);
	m_cellsToTest.push_back(0);
	IncrementTestCounter();
	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();

		const CQuadTreeCell& cell = m_cells[cellIndex];

		if (!Overlap::Point_AABB2D(point, cell.m_aabb))
			continue;

		if (cell.IsLeaf())
		{
			// if leaf test the elements
			unsigned numElements = cell.m_elementIndices.size();
			for (unsigned i = 0; i != numElements; ++i)
			{
				int elementIndex = cell.m_elementIndices[i];
				const SAIQuadTreeElement& element = m_elements[elementIndex];
				// see if we've already done this triangle
				if (element.m_counter == m_testCounter)
					continue;
				element.m_counter = m_testCounter;
				if (element.DoesAABBIntersectPoint(point))
					elements.push_back(&element);
			}
		}
		else
		{
			// if non-leaf, just add the children,
			for (unsigned iChild = 0; iChild < CQuadTreeCell::NUM_CHILDREN; ++iChild)
			{
				int childIndex = cell.m_childCellIndices[iChild];
				m_cellsToTest.push_back(childIndex);
			}
		}
	}

	return elements.size();
}

//===================================================================
// GetElements
//===================================================================
template<typename SAIQuadTreeElement>
unsigned CAIQuadTree<SAIQuadTreeElement >::GetElements(std::vector<const SAIQuadTreeElement*>& elements, const AABB& aabb) const
{
	elements.resize(0);

	if (m_cells.empty())
		return 0;

	m_cellsToTest.resize(0);
	m_cellsToTest.push_back(0);
	IncrementTestCounter();
	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();

		const CQuadTreeCell& cell = m_cells[cellIndex];

		if (!Overlap::AABB_AABB2D(aabb, cell.m_aabb))
			continue;

		if (cell.IsLeaf())
		{
			// if leaf test the elements
			unsigned numElements = cell.m_elementIndices.size();
			for (unsigned i = 0; i != numElements; ++i)
			{
				int elementIndex = cell.m_elementIndices[i];
				const SAIQuadTreeElement& element = m_elements[elementIndex];
				// see if we've already done this triangle
				if (element.m_counter == m_testCounter)
					continue;
				element.m_counter = m_testCounter;
				if (element.DoesAABBIntersectAABB(aabb))
					elements.push_back(&element);
			}
		}
		else
		{
			// if non-leaf, just add the children,
			for (unsigned iChild = 0; iChild < CQuadTreeCell::NUM_CHILDREN; ++iChild)
			{
				int childIndex = cell.m_childCellIndices[iChild];
				m_cellsToTest.push_back(childIndex);
			}
		}
	}

	return elements.size();
}

//===================================================================
// DumpCell
//===================================================================
template<typename SAIQuadTreeElement>
void CAIQuadTree<SAIQuadTreeElement >::DumpCell(const CQuadTreeCell& cell, int indentLevel) const
{
	string indent;
	for (int i = 0; i < indentLevel; ++i)
		indent += " ";
	if (cell.IsLeaf())
	{
		if (cell.m_elementIndices.empty())
		{
			AILogAlways("%s No elements", indent.c_str());
		}
		else
		{
			string output;
			string newBit;
			for (unsigned i = 0; i < cell.m_elementIndices.size(); ++i)
				AILogAlways("%s %d %s", indent.c_str(), cell.m_elementIndices[i], m_elements[cell.m_elementIndices[i]].GetDebugName());
		}
	}
	else
	{
		indentLevel += 2;
		AILogAlways("%s PP", indent.c_str());
		DumpCell(m_cells[cell.m_childCellIndices[CQuadTreeCell::PP]], indentLevel);
		AILogAlways("%s PM", indent.c_str());
		DumpCell(m_cells[cell.m_childCellIndices[CQuadTreeCell::PM]], indentLevel);
		AILogAlways("%s MP", indent.c_str());
		DumpCell(m_cells[cell.m_childCellIndices[CQuadTreeCell::MP]], indentLevel);
		AILogAlways("%s MM", indent.c_str());
		DumpCell(m_cells[cell.m_childCellIndices[CQuadTreeCell::MM]], indentLevel);
	}
}

//===================================================================
// Dump
//===================================================================
template<typename SAIQuadTreeElement>
void CAIQuadTree<SAIQuadTreeElement >::Dump(const char* debugName) const
{
	AILogAlways("QuadTree %s", debugName);
	int indentLevel = 0;

	if (m_cells.empty())
	{
		AILogAlways("  empty");
	}
	else
	{
		DumpCell(m_cells[0], 2);
	}
}
