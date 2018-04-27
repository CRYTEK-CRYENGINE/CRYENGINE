// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   main quadtree class implementations
 */

#pragma warning(disable:4127)

namespace NQT
{
template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNode &
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::GetNode(const TIndexType cIndex) const
{
	assert((size_t)cIndex < m_Nodes.size());
#if defined(_DEBUG)
	m_DebugStats.nodeArrayAccesses++;
#endif
	return m_Nodes[cIndex];
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadNode & CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::GetNode(const TIndexType cIndex)
{
	assert((size_t)cIndex < m_Nodes.size());
#if defined(_DEBUG)
	m_DebugStats.nodeArrayAccesses++;
#endif
	return m_Nodes[cIndex];
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::TLeaf &
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::GetLeaf(const TIndexType cIndex) const
{
	assert((size_t)cIndex < m_Leaves.size());
#if defined(_DEBUG)
	m_DebugStats.leafArrayAccesses++;
#endif
	return m_Leaves[cIndex];
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::TLeaf &
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::GetLeaf(const TIndexType cIndex)
{
	assert((size_t)cIndex < m_Leaves.size());
#if defined(_DEBUG)
	m_DebugStats.leafArrayAccesses++;
#endif
	return m_Leaves[cIndex];
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetLeafCapacity() const
{
#if defined(_DEBUG)
	m_DebugStats.Reset();
#endif
	return (TIndexType)min((uint64)MaxElems(), (uint64)(TMaxCellElems * (uint32)pow(4.f, (int)m_MaxSubDivLevels)));
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::MaxElems() const
{
#if defined(_DEBUG)
	m_DebugStats.Reset();
#endif
	const uint32 cMaxMemoryAvail = 64 * 1024 * 1024;
	return std::min((TIndexType)(((uint64)2 << (sizeof(TIndexType) * 8 - 2)) - (TIndexType)1), (TIndexType)(cMaxMemoryAvail / sizeof(TLeaf)));//first bit reserved for leave/node indication
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::~CQuadTree()
{}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::SetMaxSubdivLevels(const uint16 cLevels)
{
	m_MaxSubDivLevels = cLevels;
	float sideExtension = (float)m_SideLength;
	const float cMinSubDivDist = GetMinSubdivDist(SType2Type<TPosType>());
	for (uint16 i = 0; i < cLevels; ++i)
	{
		if (sideExtension < cMinSubDivDist)
		{
			m_MaxSubDivLevels = i;
			break;
		}
		sideExtension *= 0.5f;
	}
	m_MinSideLength = sideExtension;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const uint16 CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetMaxSubdivLevels() const
{
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
	m_DebugStats.Reset();
#endif
	return m_MaxSubDivLevels;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::CQuadTree
(
  const TPosType cSideExtension,
  const TVec2& crCenterPosition,
  const uint16 cMaxSubDivLevels,
  const uint32 cReservedNodeMem,
  const uint32 cReservedLeafMem
)
	: m_SideLength(cSideExtension), m_Center(TVec2F(crCenterPosition.x, crCenterPosition.y)), m_UniqueLeaveCount(0)
{
	assert(TMaxCellElems > 0);                       //do not instantiate with bad template param (0 cell leaf count)
	assert(sizeof(SQuadNode) == sizeof(TIndexType)); //if this fails, a pragma pack needs to be added to the SLeaf structure to avoid memory overheads
	if (TUseRadius)
		InitializeRadiusCompression();

	BuildSliceLookupTable();

	//reserve enough memory
	const uint32 cReservedNodeCount = cReservedNodeMem / sizeof(SQuadNode);
	if (cReservedNodeCount > 0)
		m_Nodes.reserve(cReservedNodeCount);
	const uint32 cReservedLeafCount = cReservedLeafMem / sizeof(TLeaf);
	if (cReservedLeafCount > 0)
		m_Leaves.reserve(cReservedLeafCount);
#if defined(_DEBUG)
	m_IsInitialized = true;
#endif
	InsertRootNode();
	//set max subdivision levels
	SetMaxSubdivLevels(cMaxSubDivLevels);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
CQuadTree(const uint32 cReservedNodeMem, const uint32 cReservedLeafMem)
{
	assert(TMaxCellElems > 0);                       //do not instantiate with bad template param (0 cell leaf count)
	assert(sizeof(SQuadNode) == sizeof(TIndexType)); //if this fails, a pragma pack needs to be added to the SLeaf structure to avoid memory overheads
#if defined(_DEBUG)
	m_IsInitialized = false;
#endif

	//reserve enough memory
	//reserve enough memory
	const uint32 cReservedNodeCount = cReservedNodeMem / sizeof(SQuadNode);
	if (cReservedNodeCount > 0)
		m_Nodes.reserve(cReservedNodeCount);
	const uint32 cReservedLeafCount = cReservedLeafMem / sizeof(TLeaf);
	if (cReservedLeafCount > 0)
		m_Leaves.reserve(cReservedLeafCount);

	BuildSliceLookupTable();
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::BuildSliceLookupTable()
{
	//lookup table will contain lots of invalid values, but those are never accessed
	const float cStep = 2.f / (float)scLookupCount;  //-1..1
	const float cPi = 3.14159265f;
	const float cAngleQuantInv = (float)scInitalSliceGranularity / (2.00001f * cPi);
	for (int x = 0; x < 2; ++x)//sign
	{
		float curY = -1.f + cStep * 0.5f;  //center it
		for (int y = 0; y < scLookupCount; ++y)//y
		{
			float angle = (x == 0) ? asinf(curY) : (cPi - asinf(curY));
			if (angle < 0)
				angle += (float)(2.f * cPi);
			assert(angle >= 0.f && angle <= 2.f * cPi);
			const float cAngleQuantIndex = angle * cAngleQuantInv;
			if (cAngleQuantIndex - floor(cAngleQuantIndex) >= 0.5f)
			{
				m_SliceLookupTable[y][x].first = ((uint8)cAngleQuantIndex + 1) & (scInitalSliceGranularity - 1);
				m_SliceLookupTable[y][x].second = ((uint8)cAngleQuantIndex) & (scInitalSliceGranularity - 1);
			}
			else
			{
				m_SliceLookupTable[y][x].first = ((uint8)cAngleQuantIndex) & (scInitalSliceGranularity - 1);
				m_SliceLookupTable[y][x].second = ((uint8)cAngleQuantIndex + 1) & (scInitalSliceGranularity - 1);
			}
			curY += cStep;
		}
	}
	m_SliceLookupTable[scLookupCount][0] = m_SliceLookupTable[scLookupCount - 1][0];
	m_SliceLookupTable[scLookupCount][1] = m_SliceLookupTable[scLookupCount - 1][1];
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::Init
(
  const TPosType cSideExtension,
  const uint16 cMaxSubDivLevels,
  const TVec2& crCenterPosition
)
{
#if defined(_DEBUG)
	assert(m_IsInitialized == false);
#endif
	m_SideLength = cSideExtension;
	if (TUseRadius)
		InitializeRadiusCompression();
	m_Center = TVec2F(crCenterPosition.x, crCenterPosition.y);
#if defined(_DEBUG)
	m_IsInitialized = true;
#endif
	m_UniqueLeaveCount = 0;
	InsertRootNode();
	//set max subdivision levels
	SetMaxSubdivLevels(cMaxSubDivLevels);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const float CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetRadiusFromLeaf(const TLeaf& crLeaf) const
{
	return (float)crLeaf.GetRadius() * m_CompressionFactor;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const float CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetRadius(const TRadiusType cComprRadius) const
{
	return (float)cComprRadius * m_CompressionFactor;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const float CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetRadiusFromLeaf(const TIndexType cLeafIndex) const
{
	const TLeaf& crLeaf = GetLeaf();
	return (float)crLeaf.GetRadius() * m_CompressionFactor;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::TRadiusType
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::CompressRadius(const float cRadius) const
{
	assert(cRadius < m_MaxRadius);
	return (TRadiusType)(cRadius / m_CompressionFactor) + 1;  //pad 1 to compensate for float truncation
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::InitializeRadiusCompression()
{
	//set max radius to the side length of the 3rd subdivision depth, if not enough, change that further later on
	m_MaxRadius = m_SideLength * 0.125f;
	m_CompressionFactor = m_MaxRadius / (float)((2 << (sizeof(TRadiusType) * 8 - 1)) - 1);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::LeafCount() const
{
#if defined(_DEBUG)
	m_DebugStats.Reset();
#endif
	return m_UniqueLeaveCount;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::NodeCount() const
{
#if defined(_DEBUG)
	m_DebugStats.Reset();
	m_DebugStats.nodeArrayAccesses++;
#endif
	return (TIndexType)m_Nodes.size();
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const size_t CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::ConsumedMemory() const
{
#if defined(_DEBUG)
	m_DebugStats.Reset();
#endif
	return (sizeof(*this) + m_DeletedLeaves.size() * sizeof(TIndexType) + m_Leaves.size() * sizeof(SQuadLeaf<TLeafContent, TIndexType> ) + m_Nodes.size() * sizeof(SQuadNode));
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::OptimizeTree()
{
	//reallocate vectors to exact size, reinsert all leaves to not have any unnecessary subdivision
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
	m_DebugStats.Reset();
#endif
#if defined(_DEBUG)
	const TIndexType cLeafCount = m_UniqueLeaveCount;
#endif
	const size_t cLeafArraySize = m_Leaves.size();
	m_UniqueLeaveCount = 0;
	TLeafVect newLeaves;
	TNodeVec newNodes;
	newLeaves.swap(m_Leaves);
	newNodes.swap(m_Nodes);
	m_Leaves.reserve(cLeafArraySize); //leaf count should remain the same, only node count might decrease
	InsertRootNode();                 //insert root node
	//now insert all non deleted leaves again, but only once (stored multiple times if TUseRadius)
	std::set<TLeafContent> leavesSet;
	const typename std::set<TIndexType>::const_iterator cSetEnd = m_DeletedLeaves.end();
	for (TIndexType index = 0; index < newLeaves.size(); ++index)
	{
		const TLeaf& crLeaf = newLeaves[index];
		if (leavesSet.find(crLeaf.GetContent()) != leavesSet.end())
			continue;  //already inserted
		leavesSet.insert(crLeaf.GetContent());
		if (m_DeletedLeaves.find(index) != cSetEnd)
			continue;  //marked as deleted
		const EInsertionResult cInsertionRes = InsertLeaf(crLeaf.GetPos(), crLeaf.GetContent(), GetRadiusFromLeaf(crLeaf), false);
		assert(cInsertionRes == SUCCESS);
#if defined(_DEBUG)
		assert(cLeafCount >= m_UniqueLeaveCount);  //there should not be more leaves in the tree than before
#endif
	}
	m_DeletedLeaves.clear();
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetCellLeafCount(const SQuadNode& crNode, std::vector<TIndexType>& rCellLeaves) const
{
	rCellLeaves.resize(0);
	if (!crNode.HasLeafNodes())
		return 0;
	TIndexType count = 1;
	TIndexType cellLeafIndex = crNode.GetLeafIndex();
	assert(cellLeafIndex < (TIndexType)m_Leaves.size());
	rCellLeaves.push_back(cellLeafIndex);
	for (int i = 0; i < TMaxCellElems - 1; ++i)
	{
		const TLeaf& crLeaf = GetLeaf(cellLeafIndex);
		cellLeafIndex = crLeaf.GetNextCellLeaf();
		if (cellLeafIndex == TLeaf::INVALID_LEAF_INDEX)
			break;
		rCellLeaves.push_back(cellLeafIndex);
		++count;
	}
	return count;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::InsertRootNode()
{
	assert(m_Nodes.empty());
	m_Nodes.push_back(SQuadNode());
#if defined(_DEBUG)
	m_DebugStats.nodeArrayAccesses++;
#endif
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
Subdivide(const float cQuadSideLength, const TVec2F& crCenterPos, SQuadNode& rQuadNodeToSubdiv, const uint32 cLevels)
{
#if defined(_DEBUG)
	assert(cLevels > 0);
	const size_t cOldNodeCount = m_Nodes.size();
#endif
#if defined(_DEBUG)
	m_DebugStats.nodeArrayAccesses++;
#endif
	if (m_Nodes.size() + 4 > (size_t)MaxElems())
		return false;
	//call subdivide for this node and call it with one level less or each child
	const TIndexType cChildIndex = Subdivide(crCenterPos, rQuadNodeToSubdiv);
	if (cChildIndex == INVALID_CHILD_NODE)
		return false;
#if defined(_DEBUG)
	assert(m_Nodes.size() == cOldNodeCount + 4);
#endif
	if (cLevels == 1) //done
		return true;
	TVec2F childCenters[4];
	//retrieve new center positions
	GetSubdivisionCenters(cQuadSideLength, crCenterPos, childCenters);
	const float cChildSideLength = cQuadSideLength / 2.0f;
	for (int i = 0; i < 4; ++i)
	{
		if (!Subdivide(cChildSideLength, childCenters[i], GetNode(cChildIndex + i), cLevels - 1))
			return false;
	}
	return true;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
Subdivide(const TVec2F& crCenterPos, SQuadNode& rQuadNodeToSubdiv)
{
	//pay attention, reference to rQuadNodeToSubdiv might be invalid after function exiting due to node array allocation
	SQuadNode lU, rU, lL, rL;   //new nodes
	const TIndexType cFirstChildIndex = (TIndexType)m_Nodes.size();
#if defined(_DEBUG)
	m_DebugStats.nodeArrayAccesses++;
#endif
	if (cFirstChildIndex + 4 > MaxElems())
		return INVALID_CHILD_NODE;
	rQuadNodeToSubdiv.childNodeIndex = cFirstChildIndex;  //automatically removes HAS_LEAF_FLAG
	//add nodes and set index to children
	m_Nodes.push_back(lU);
	m_Nodes.push_back(rU);
	m_Nodes.push_back(lL);
	m_Nodes.push_back(rL);
#if defined(_DEBUG)
	m_DebugStats.nodeArrayAccesses += 4;
#endif
	return cFirstChildIndex;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
RetrieveLeafByIndex(const TIndexType cLeafIndex, TLeaf& rLeaf) const
{
	assert(m_DeletedLeaves.find(cLeafIndex) == m_DeletedLeaves.end());
	if (cLeafIndex < (TIndexType)m_Leaves.size())
	{
		rLeaf = GetLeaf(cLeafIndex);
#if defined(_DEBUG)
		m_DebugStats.leafArrayAccesses++;
#endif
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetLeafArrayIndex(const TLeafContent& crCont, const TVec2& crPos, TIndexType& rParentNodeIndex, TTravRecVecType& rTraversedNodes) const
{
	//walk down the quadtree to retrieve right element
	//check recursively which cell index the pos lies in
	assert(m_Nodes.size() > 0);
	SQuadNodeData nodeData;
	uint16 subDivLevel = 0;
	RetrieveDeepestNode(crPos, (float)m_SideLength, m_Center, nodeData, 0, rTraversedNodes, subDivLevel);
	rParentNodeIndex = nodeData.nodeIndex;
	const SQuadNode& crParentNode = GetNode(rParentNodeIndex);
	TIndexType leafIndex = (crParentNode.HasLeafNodes()) ? crParentNode.GetLeafIndex() : TLeaf::INVALID_LEAF_INDEX;
	if (leafIndex != TLeaf::INVALID_LEAF_INDEX)
	{
		//has found leaf node
		//now retrieve the leaf with the same content
		TLeaf leaf;
		for (;; )
		{
			RetrieveLeafByIndex(leafIndex, leaf);
			if (leaf.GetContent() == crCont && leaf.GetPos() == crPos)
				return leafIndex;
			leafIndex = leaf.GetNextCellLeaf();
			if (leafIndex == TLeaf::INVALID_LEAF_INDEX)
				break;
		}
	}
	return TLeaf::INVALID_LEAF_INDEX;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::RetrieveDeepestNode
(
  const TVec2F& crPos,
  const float cSideLength,
  const TVec2F& crCenter,
  SQuadNodeData& rNodeData,
  const TIndexType cStartNodeIndex,
  TTravRecVecType& rTraversedNodes,
  uint16& rSubdivisonLevel
) const
{
	assert(abs(crPos.x - m_Center.x) <= m_SideLength / 2.0f && abs(crPos.y - m_Center.y) <= m_SideLength / 2.0f);
	const SQuadNode& crStartNode = GetNode(cStartNodeIndex);
	//checks current child nodes and if it is no leaf, get the leaf crPos is in its cell via GetQuadrantIndex and recurse
	if (crStartNode.HasLeafNodes() || !crStartNode.HasChildNodes())
	{
		rNodeData.nodeIndex = cStartNodeIndex;
		rNodeData.centerPos = crCenter;
		rNodeData.sideLength = cSideLength;
		return;
	}
	//else check which leaf crPos is in its quadrant
	const TEChildIndex cCellIndex = GetQuadrantIndex(crCenter, crPos);
	const TIndexType cFirstChildIndex = crStartNode.childNodeIndex;
	assert(cFirstChildIndex != 0);
	TVec2F childCenters[4];
	//retrieve new center positions
	GetSubdivisionCenters(cSideLength, crCenter, childCenters);
	//record traversed node if it is not the final node
	rTraversedNodes.push_back(std::make_pair(SQuadNodeData(cStartNodeIndex, crCenter, cSideLength), cCellIndex));
	rSubdivisonLevel++;
	RetrieveDeepestNode(crPos, cSideLength / 2.0f, childCenters[(int)cCellIndex], rNodeData, cFirstChildIndex + cCellIndex, rTraversedNodes, rSubdivisonLevel);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<uint32 TNewMaxCellElems, class TNewIndexType>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
CQuadTree(const CQuadTree<TLeafContent, TNewMaxCellElems, TPosType, TNewIndexType, TUseRadius>& crConvertFrom)
	: m_SideLength(crConvertFrom.m_SideLength), m_Center(crConvertFrom.m_Center), m_MaxSubDivLevels(crConvertFrom.m_MaxSubDivLevels)
{
#if defined(_DEBUG)
	m_DebugStats.Reset();
	m_IsInitialized = true;
#endif
	if (TUseRadius)
		InitializeRadiusCompression();
	//simply reinsert all elements and copy center plus side-length
	//reserve enough memory first
	m_Leaves.reserve((size_t)crConvertFrom.LeafCount());
	InsertRootNode();
	assert(crConvertFrom.LeafCount() < MaxElems());  //otherwise cant convert
	std::set<TLeafContent> leavesSet;
#if defined(_DEBUG)
	const TIndexType cLeafCount = crConvertFrom.m_UniqueLeaveCount;
#endif
	m_UniqueLeaveCount = 0;
	if (crConvertFrom.LeafCount() < MaxElems())
	{
		//now insert all leaves
		const typename CQuadTree<TLeafContent, TNewMaxCellElems, TPosType, TNewIndexType, TUseRadius>::
		TLeafVect::const_iterator leafEndIter = crConvertFrom.m_Leaves.end();
		TNewIndexType index = 0;
		const typename std::set<TNewIndexType>::const_iterator cSetEnd = crConvertFrom.m_DeletedLeaves.end();
		for (; index != crConvertFrom.m_Leaves.size(); ++index)
		{
			const TLeaf& crLeaf = crConvertFrom.GetLeaf(index);
			if (leavesSet.find(crLeaf.GetContent()) != leavesSet.end())
				continue;  //already inserted
			leavesSet.insert(crLeaf.GetContent());
			if (crConvertFrom.m_DeletedLeaves.find(index) != cSetEnd)
				continue;  //marked as deleted
			InsertLeaf(crLeaf.GetPos(), crLeaf.GetContent(), crConvertFrom.GetRadiusFromLeaf(index));
#if defined(_DEBUG)
			assert(cLeafCount >= m_UniqueLeaveCount);  //there should not be more leaves in the tree than before
#endif
		}
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
RemoveLeafFromNode(SQuadNode& rParentNode, const TIndexType cLeafIndex)
{
	assert(rParentNode.HasLeafNodes());
	//now iterate all leaves within that cell
#if defined(_DEBUG)
	bool found = false;  //check whether leaf has really been found
#endif
	TIndexType cellLeafIndex = rParentNode.GetLeafIndex();                   //first leaf index in that cell
	TIndexType nextCellLeafIndex = GetLeaf(cellLeafIndex).GetNextCellLeaf(); //next leaf index within that cell
	if (cellLeafIndex == cLeafIndex)                                         //is the first cell leaf index the right one already?
	{
		if (nextCellLeafIndex == TLeaf::INVALID_LEAF_INDEX)
			rParentNode.childNodeIndex = 0;
		else
			rParentNode.SetLeafIndex(nextCellLeafIndex);
#if defined(_DEBUG)
		found = true;
#endif
	}
	else
	{
		for (int i = 0; i < TMaxCellElems - 1; ++i)
		{
			TIndexType tempNextCellLeaf = 0;
			if (nextCellLeafIndex != TLeaf::INVALID_LEAF_INDEX)
			{
				tempNextCellLeaf = GetLeaf(nextCellLeafIndex).GetNextCellLeaf();
				if (nextCellLeafIndex == cLeafIndex)
				{
					if (tempNextCellLeaf == TLeaf::INVALID_LEAF_INDEX)
						GetLeaf(cellLeafIndex).SetNextCellLeaf(0, false);  //finish linkage
					else
						GetLeaf(cellLeafIndex).SetNextCellLeaf(tempNextCellLeaf);  //link to next one
#if defined(_DEBUG)
					found = true;
#endif
					break;
				}
			}
			else
				break;
			cellLeafIndex = nextCellLeafIndex;
			assert(cellLeafIndex < (TIndexType)m_Leaves.size());
			nextCellLeafIndex = tempNextCellLeaf;
		}
	}
#if defined(_DEBUG)
	assert(found);
#endif
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
RemoveLeafFromNode(SQuadNode& rParentNode, const TLeafContent& crCont, const TVec2& crPos)
{
	TIndexType ret = TLeaf::INVALID_LEAF_INDEX;
	assert(rParentNode.HasLeafNodes());
	//now iterate all leaves within that cell
	TIndexType cellLeafIndex = rParentNode.GetLeafIndex();                                         //first leaf index in that cell
	TIndexType nextCellLeafIndex = GetLeaf(cellLeafIndex).GetNextCellLeaf();                       //next leaf index within that cell
	if (GetLeaf(cellLeafIndex).GetPos() == crPos && GetLeaf(cellLeafIndex).GetContent() == crCont) //is the first cell leaf index the right one already?
	{
		ret = cellLeafIndex;
		if (nextCellLeafIndex == TLeaf::INVALID_LEAF_INDEX)
			rParentNode.childNodeIndex = 0;
		else
			rParentNode.SetLeafIndex(nextCellLeafIndex);
	}
	else
	{
		for (int i = 0; i < TMaxCellElems - 1; ++i)
		{
			TIndexType tempNextCellLeaf = 0;
			;
			if (nextCellLeafIndex != TLeaf::INVALID_LEAF_INDEX)
			{
				tempNextCellLeaf = GetLeaf(nextCellLeafIndex).GetNextCellLeaf();
				if (GetLeaf(nextCellLeafIndex).GetPos() == crPos && GetLeaf(nextCellLeafIndex).GetContent() == crCont)
				{
					ret = nextCellLeafIndex;
					tempNextCellLeaf = GetLeaf(nextCellLeafIndex).GetNextCellLeaf();
					if (tempNextCellLeaf == TLeaf::INVALID_LEAF_INDEX)
						GetLeaf(cellLeafIndex).SetNextCellLeaf(0, false);  //finish linkage
					else
						GetLeaf(cellLeafIndex).SetNextCellLeaf(tempNextCellLeaf);  //link to next one
					break;
				}
			}
			else
				break;
			cellLeafIndex = nextCellLeafIndex;
			assert(cellLeafIndex < (TIndexType)m_Leaves.size());
			nextCellLeafIndex = tempNextCellLeaf;
		}
	}
	return ret;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::EnsureTreeIntegrity(const TTravRecVecType crTraversedNodes)
{
	const uint32 cCount = crTraversedNodes.size();
	for (int i = cCount - 1; i >= 0; --i)
	{
		//if all 4 children have no leaves or children, set it also to 0
		SQuadNode& rNode = GetNode(crTraversedNodes[i].first.nodeIndex);
		if (rNode.childNodeIndex != 0)
		{
			if (!GetNode(rNode.childNodeIndex).childNodeIndex &&
			    !GetNode(rNode.childNodeIndex + 1).childNodeIndex &&
			    !GetNode(rNode.childNodeIndex + 2).childNodeIndex &&
			    !GetNode(rNode.childNodeIndex + 3).childNodeIndex)
				rNode.childNodeIndex = 0;  //indicate no children anymore
		}
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::RemoveLeaf
(
  const TLeafContent& crCont, const TVec2& crPos
)
{
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
	m_DebugStats.Reset();
#endif
	TIndexType parentNodeIndex;
	TTravRecVecType traversedNodes;
	const TIndexType cLeafIndex = GetLeafArrayIndex(crCont, crPos, parentNodeIndex, traversedNodes);
	SQuadNode& rParentNode = GetNode(parentNodeIndex);
	if (cLeafIndex == TLeaf::INVALID_LEAF_INDEX)
		return false;
	if (TUseRadius)
	{
		//in case of radius usage, retrieve radius too
		const float cRadius = GetRadiusFromLeaf(GetLeaf(cLeafIndex));
		TNodeDataVec nodeVec;
		GatherAllRadiusSharingNodes(nodeVec, SQuadNodeData(0, m_Center, (float)m_SideLength), crPos, cRadius);
		//now remove leaf from all shared nodes
		const typename TNodeDataVec::const_iterator cEnd = nodeVec.end();
		for (typename TNodeDataVec::const_iterator iter = nodeVec.begin(); iter != cEnd; ++iter)
		{
			//remove index from parent node and relink leaves within the same cell
			SQuadNode& rNode = GetNode(iter->first.nodeIndex);
			const TIndexType cLeafIndex = RemoveLeafFromNode(rNode, crCont, crPos);
			//now mark leaf as deleted
			assert(m_DeletedLeaves.find(cLeafIndex) == m_DeletedLeaves.end());    //wanna delete already deleted node?
			if (m_DeletedLeaves.find(cLeafIndex) != m_DeletedLeaves.end())
				return false;
			m_DeletedLeaves.insert(cLeafIndex);
		}
		//now ensure tree integrity, do it now after removing leaf from all sharing nodes to not do it several times
		for (typename TNodeDataVec::const_iterator iter = nodeVec.begin(); iter != cEnd; ++iter)
		{
			//remove index from parent node and relink leaves within the same cell
			const SQuadNodeData& crNodeData = iter->first;
			const SQuadNode& crNode = GetNode(crNodeData.nodeIndex);
			if (crNode.childNodeIndex == 0)
			{
				SQuadNodeData nodeData;
				TTravRecVecType traversedNodeNodes;
				uint16 subDivLevel = 0;
				RetrieveDeepestNode(crNodeData.centerPos, (float)m_SideLength, m_Center, nodeData, 0, traversedNodeNodes, subDivLevel);
				EnsureTreeIntegrity(traversedNodeNodes);
			}
		}
	}
	else
	{
		//now mark leaf as deleted and remove index from parent node and relink leaves within the same cell
		assert(m_DeletedLeaves.find(cLeafIndex) == m_DeletedLeaves.end());    //wanna delete already deleted node?
		if (m_DeletedLeaves.find(cLeafIndex) != m_DeletedLeaves.end())
			return false;
		m_DeletedLeaves.insert(cLeafIndex);
		RemoveLeafFromNode(rParentNode, cLeafIndex);
		//now make sure that there exist no node without any leaves
		EnsureTreeIntegrity(traversedNodes);
	}
	assert(m_UniqueLeaveCount > 0);
	m_UniqueLeaveCount--;
	return true;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TCompType>
inline const SVec2<TCompType> CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
ComputeNextGridPoint00(const SVec2<TCompType>& crPos) const
{
	return SVec2<TCompType>
	       (
	  (TCompType)(floor((float)crPos.x / m_MinSideLength) * m_MinSideLength + 0.5 * m_MinSideLength),
	  (TCompType)(floor((float)crPos.y / m_MinSideLength) * m_MinSideLength + 0.5 * m_MinSideLength)
	       );
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const EInsertionResult CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::VerifyLeafPos
(
  const std::vector<TIndexType>& crCellLeaves,
  const TVec2& crPos,
  const SQuadNodeData& crNodeData,
  const uint16 cCurrentSubdivLevel
) const
{
	typedef typename std::vector<TIndexType>::const_iterator TCellIter;
	const TCellIter cEnd = crCellLeaves.end();
	//quick pos equality test
	if (crCellLeaves.size() < TMaxCellElems)      //can insert without care (only pos needs to be different)
	{
		for (TCellIter iter = crCellLeaves.begin(); iter != cEnd; ++iter)
		{
			//check for equality
			if (GetLeaf(*iter).GetPos() == crPos)
				return POS_EQUAL_TO_EXISTING_LEAF;
		}
		return SUCCESS;
	}
	//check for subdivision levels
	if (cCurrentSubdivLevel >= m_MaxSubDivLevels && crCellLeaves.size() >= TMaxCellElems) //cannot subdivide anymore
		return NOT_ENOUGH_SUBDIV_LEVELS;

	//do position test only in case of non radius, it gets too complicated otherwise, it is taken care when subdivision level is reached
	if (TUseRadius)
		return SUCCESS;

	//retrieve non rotated positions
	static const TVec2F cCenterPosNoRot = m_Center;   //center pos set in constructor, so this is safe and good here
	static const TVec2F cRelOffset = TVec2F(m_SideLength * 0.5, m_SideLength * 0.5) - cCenterPosNoRot;
	const TVec2 cPosNoRot(crPos);
	const TVec2F cPosRel(cPosNoRot.x + cRelOffset.x, cPosNoRot.y + cRelOffset.y);  //relative position with 0,0 in upper left corner
	std::vector<TVec2F> positions;
	positions.reserve(crCellLeaves.size());
	//retrieve non rotated positions for leaves and check for position equality
	for (TCellIter iter = crCellLeaves.begin(); iter != cEnd; ++iter)
	{
		//add non rotated positions
		const TLeaf& crLeaf = GetLeaf(*iter);
		const TVec2 cNewPosNoRot = crLeaf.GetPos();
		//check for equality
		if (crLeaf.GetPos() == crPos)
			return POS_EQUAL_TO_EXISTING_LEAF;
		positions.push_back(TVec2F(cNewPosNoRot.x, cNewPosNoRot.y));
	}
	//check whether any leaf number above of TMaxCellElems-1 will end up int the same cell as crPos (limited by GetMinSubdivDist)
	//retrieve mid point at lowest subdivision level new crPos
	const TVec2F cMidPoint(ComputeNextGridPoint00(cPosRel));
	uint32 leavesInSameCell = 0;
	const typename std::vector<TVec2F>::const_iterator cPosEnd = positions.end();
	for (typename std::vector<TVec2F>::const_iterator iter = positions.begin(); iter != cPosEnd; ++iter)
	{
		const TVec2F relPos = *iter + cRelOffset;  //relative position with 0,0 in upper left corner
		assert(relPos.x < m_SideLength && relPos.y < m_SideLength);
		if (abs(cPosRel.x - relPos.x) < m_MinSideLength || abs(cPosRel.y - relPos.y) < m_MinSideLength)
		{
			//within min subdivision distance and level, check resulting centers
			//retrieve mid point at lowest subdivision level for this leaf
			const TVec2F cLeafMidPoint(ComputeNextGridPoint00(relPos));
			if (cLeafMidPoint == cMidPoint) //same cell
				++leavesInSameCell;
			if (leavesInSameCell >= TMaxCellElems)
				return NOT_ENOUGH_SUBDIV_LEVELS;
		}
	}
	return SUCCESS;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GatherAllRadiusSharingNodes
(
  TNodeDataVec& rNodeVec,
  const SQuadNodeData& crNodeData,
  const TVec2& crPos,
  const float cRadius,
  const uint16 cSubDivLevel
) const
{
	const SQuadNode& crParentNode = GetNode(crNodeData.nodeIndex);
	if (crParentNode.HasLeafNodes() || !crParentNode.HasChildNodes())
	{
		rNodeVec.push_back(std::make_pair(crNodeData, cSubDivLevel));
		return;
	}
	//check all 4 child nodes and recurse to those the intersection test was succesful
	TVec2F childCenters[4];
	GetSubdivisionCenters(crNodeData.sideLength, crNodeData.centerPos, childCenters);
	bool childHits[4];
	int hitCount = 0;
	for (int i = 0; i < 4; ++i)
	{
		childHits[i] = IsPosInCell(crPos, crNodeData.sideLength * 0.5, childCenters[i], cRadius);
		if (childHits[i])
			hitCount++;
	}
	if (hitCount == 0)
		return;
	const float cChildSideLength = crNodeData.sideLength * 0.5f;
	for (int i = 0; i < 4; ++i)
		if (childHits[i])
			GatherAllRadiusSharingNodes(rNodeVec, SQuadNodeData(crParentNode.childNodeIndex + i, childCenters[i], cChildSideLength), crPos, cRadius, cSubDivLevel + 1);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GatherAllIntersectingNodes
(
  std::vector<const TLeafContent*>& rContents,
  const SQuadNodeData& crNodeData,
  const TVec2F& crRayPos,
  const TVec2F& crRayDir,
  const float cRayLength,
  const uint32 cMaxCount,
  uint32& rCurCount
) const
{
	const SQuadNode& crParentNode = GetNode(crNodeData.nodeIndex);
	if (crParentNode.HasLeafNodes())
		return AddIntersectingLeaves(rContents, crParentNode, crRayPos, crRayDir, cRayLength, cMaxCount, rCurCount);
	if (!crParentNode.HasChildNodes())
		return (rCurCount >= cMaxCount);
	//check all 4 child nodes and recurse to those the intersection test was succesful
	TVec2F childCenters[4];
	GetSubdivisionCenters(crNodeData.sideLength, crNodeData.centerPos, childCenters);
	TVec2F childCentersUnRot[4];
	for (int i = 0; i < 4; ++i)
		childCentersUnRot[i] = childCenters[i];
	bool childHits[4];
	const bool cHit = RayCellIntersection(crNodeData.sideLength, crRayPos, crRayDir, crNodeData.centerPos, cRayLength, childHits, childCentersUnRot);
	if (!cHit)
		return (rCurCount >= cMaxCount);
	const float cChildSideLength = crNodeData.sideLength * 0.5f;
	for (int i = 0; i < 4; ++i)
		if (childHits[i])
			if (GatherAllIntersectingNodes
			    (
			      rContents,
			      SQuadNodeData(crParentNode.childNodeIndex + i, childCenters[i], cChildSideLength),
			      crRayPos,
			      crRayDir,
			      cRayLength,
			      cMaxCount,
			      rCurCount
			    ))
				break;
	return rCurCount >= cMaxCount;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetAllContents(std::vector<std::pair<TVec2, const TLeafContent*>>& rContents) const
{
	for (typename TLeafVect::const_iterator iter = m_Leaves.begin(); iter != m_Leaves.end(); ++iter)
		rContents.push_back(std::make_pair((*iter).GetPos(), (*iter).GetContentPointer()));
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetRayIntersectionContentsOutside
(
  std::vector<const TLeafContent*>& rContents,
  const TVec2F& crPos,
  const TVec2F& crDir,
  const float cRayLength,
  const uint32 cMaxCount
) const
{
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
	m_DebugStats.Reset();
#endif
	assert(TUseRadius);
	if (!TUseRadius)
		return;  //not supported in that mode
	//gather all cells along ray
	uint32 curCount = 0;
	GatherAllIntersectingNodes(rContents, SQuadNodeData(0, m_Center, m_SideLength), crPos, crDir, cRayLength, cMaxCount, curCount);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::AddIntersectingLeaves
(
  std::vector<const TLeafContent*>& rContents,
  const SQuadNode& crNode,
  const TVec2F& crPos,
  const TVec2F& crDir,
  const float cRayLength,
  const uint32 cMaxCount,
  uint32& rCurCount
) const
{
	if (!crNode.HasLeafNodes())
		return rCurCount >= cMaxCount;  //node was in empty tree (can happen since it was chosen an arbitrary origin which can be inside an empty child node)
	//get all leaves and check with RayLeafIntersection
	TIndexType cellLeafIndex = crNode.GetLeafIndex();
	for (int i = 0; i < TMaxCellElems; ++i)
	{
		const TLeaf& crLeaf = GetLeaf(cellLeafIndex);
		//check if we have already inserted this leaf
		const TLeafContent* cpContent = crLeaf.GetContentPointer();
		bool bAlreadyInserted = false;
		const typename std::vector<const TLeafContent*>::const_iterator cEnd = rContents.end();
		for (typename std::vector<const TLeafContent*>::const_iterator iter = rContents.begin(); iter != cEnd; ++iter)
		{
			if (**iter == *cpContent)
			{
				bAlreadyInserted = true;
				break;
			}
		}
		//now check leaf if it has not been inserted
		if (!bAlreadyInserted && RayLeafIntersection(GetRadiusFromLeaf(crLeaf), crPos, crDir, crLeaf.GetPos(), cRayLength))
		{
			rContents.push_back(cpContent);
			if (++rCurCount >= cMaxCount)
				return true;
		}
		cellLeafIndex = crLeaf.GetNextCellLeaf();
		if (cellLeafIndex == TLeaf::INVALID_LEAF_INDEX)
			break;
	}
	return false;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetRayIntersectionContents
(
  std::vector<const TLeafContent*>& rContents,
  const TVec2F& crPos,
  const TVec2F& crDir,
  const float cRayLength,
  const uint32 cMaxCount
) const
{
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
	m_DebugStats.Reset();
#endif
	assert(TUseRadius);
	if (!TUseRadius)
		return;  //not supported in that mode
	rContents.resize(0);
	//idea: start at node where ray origin is in, add leaves and fetch recursively nodes toward root node
	//for each leaf node added, check and add leaves til cMaxCount is reached
	SQuadNodeData nodeData;
	TTravRecVecType traversedNodes;
	uint16 subDivLevel = 0;
	uint32 insertedCount = 0;
	RetrieveDeepestNode(crPos, (float)m_SideLength, m_Center, nodeData, 0, traversedNodes, subDivLevel);    //find node
	const SQuadNode& crNode = GetNode(nodeData.nodeIndex);
	//first add contents of current node
	if (AddIntersectingLeaves(rContents, crNode, crPos, crDir, cRayLength, cMaxCount, insertedCount))
		return;

	//now recursively check the nodes and add leaves til max count is reached or no more nodes are intersecting
	const uint32 cCount = traversedNodes.size();
	for (int i = cCount - 1; i >= 0; --i)
	{
		//traverse all nodes from bottom to the top, invoke with ::Type cast to be able to call for uint32 as index type too
		GetNodeRayIntersectionContents(traversedNodes[i], rContents, crPos, crDir, cRayLength, cMaxCount, insertedCount);
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GatherChildInfo
(
  const TVec2& crPos,
  const TVec2F& crNodeCenter,
  TChildIndices aChildIndices[TMaxCellElems + 1],
  const std::vector<TIndexType>& crCellLeaves,
  const float cRadius,
  const TVec2F cChildCenters[4],
  const float cChildNodeSideLength,
  SArraySelector<TUseRadius, bool>& rSubdivisionInfo,
  TEChildIndexExt& rPosChildIndex
)
{
	if (TUseRadius)
	{
		rPosChildIndex = NP_E;
		TEChildIndexExt posQuadrantIndex = (TEChildIndexExt)GetQuadrantIndex(crNodeCenter, crPos);
		//each leaf can go into 1..4 child cells
		//rSubdivisionInfo[0..3] is determined by counting the cells going there, if count is less than TMaxCellElems, it gets set to false
		for (int cell = 0; cell < 4; cell++)
		{
			int cellCount = 0;
			if (IsPosInCell(crPos, cChildNodeSideLength, cChildCenters[cell], cRadius))
			{
				aChildIndices[0].indices[aChildIndices[0].count++] = (TEChildIndex)cell;  //goes into child cell
				cellCount++;
				if ((int)posQuadrantIndex == cell)
				{
					if (IsPosInCell(crPos, cChildNodeSideLength, cChildCenters[cell]))
						rPosChildIndex = posQuadrantIndex;  //pos is really in that cell (quadrant index does not check for being outside a cell)
				}
			}
			for (int index = 1; index <= (int)TMaxCellElems; ++index)
			{
				//unlink leaf and determine which cell it belongs
				TLeaf& rLeaf = GetLeaf(crCellLeaves[index - 1]);
				rLeaf.SetNextCellLeaf(0, false);  //reset next leaf index
				if (IsPosInCell(rLeaf.GetPos(), cChildNodeSideLength, cChildCenters[cell], GetRadiusFromLeaf(rLeaf)))
				{
					aChildIndices[index].indices[aChildIndices[index].count++] = (TEChildIndex)cell;  //goes into child cell
					cellCount++;
				}
			}
			rSubdivisionInfo.elems[cell] = (cellCount == TMaxCellElems + 1);//true if all leaves still go there
		}
	}
	else
	{
		rSubdivisionInfo.elems[0] = true;
		//each leaf can only go into one child cell
		aChildIndices[0].indices[0] = GetQuadrantIndex(crNodeCenter, crPos); //count is fixed at 1
		rPosChildIndex = (TEChildIndexExt)aChildIndices[0].indices[0];       //safe
		for (int index = 1; index <= (int)TMaxCellElems; ++index)
		{
			//unlink leaf and determine which cell it belongs
			TLeaf& rLeaf = GetLeaf(crCellLeaves[index - 1]);
			rLeaf.SetNextCellLeaf(0, false);  //resets the previous linkage
			aChildIndices[index].indices[0] = GetQuadrantIndex(crNodeCenter, rLeaf.GetPos());
		}
		//find out whether all indices are equal, if so, recurse to next subdivision level for appropriate element
		TEChildIndex lastIndex = aChildIndices[0].indices[0];
		for (int index = 1; index <= (int)TMaxCellElems; ++index)
		{
			TEChildIndex newIndex = aChildIndices[index].indices[0];
			if (newIndex != lastIndex)
			{
				rSubdivisionInfo.elems[0] = false;
				break;
			}
		}
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
VerifyInsertionLeafState(TIndexType& rLeafIndex, TChildIndices& rChildIndex)
{
	if (rChildIndex.reinserted)
	{
		if (m_Leaves.size() >= MaxElems())
			return false;
		else
			assert(m_Leaves.size() < MaxElems());
		m_Leaves.push_back(GetLeaf(rLeafIndex).Clone());
		rLeafIndex = (TIndexType)(m_Leaves.size() - 1);
	}
	else
		rChildIndex.reinserted = true;
	return true;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
PropagateLeavesToNewNode
(
  TChildIndices aChildIndices[TMaxCellElems + 1],
  const TIndexType cFirstChildIndex,
  const std::vector<TIndexType>& crCellLeaves,
  const TIndexType cNewLeafIndex,
  const SArraySelector<TUseRadius, bool>& crNodeInsertionMask  //if element is true, node has been masked to not insert there
)
{
	//propagate all leaves, hold indices of all 4 leaves for each where the last leaf went to (to not go through the linked list all time)
#if defined(_DEBUG)
	TIndexType lastIndices[4];
	typename TNodeVec::const_iterator nodeIter = m_Nodes.begin() + (size_t)cFirstChildIndex;
	for (int i = 0; i < 4; ++i)
	{
		const SQuadNode& crNode = *nodeIter++;
		if (!TUseRadius)
			assert(!crNode.HasLeafNodes());  //should just recently been subdivided
		lastIndices[i] = INVALID_CHILD_NODE;
	}
#else
	TIndexType lastIndices[4] = { INVALID_CHILD_NODE, INVALID_CHILD_NODE, INVALID_CHILD_NODE, INVALID_CHILD_NODE };
#endif
	assert(TMaxCellElems == crCellLeaves.size());
	for (int index = 0; index <= (int)TMaxCellElems; ++index) //assign all leaves to all cells (according to aChildIndices)
	{
		TIndexType leafIndex = (index == 0) ? cNewLeafIndex : crCellLeaves[index - 1];//can change if to clone
		//iterate all nodes it should be assigned to
		//we got to clone leaves if we have to insert them multiple times
		for (int c = 0; c < aChildIndices[index].count; ++c)
		{
			TEChildIndex newIndex = aChildIndices[index].indices[c];
			if (TUseRadius && crNodeInsertionMask.elems[newIndex]) //cell is masked
				continue;
			assert((size_t)newIndex < 4);
			if (lastIndices[(int)newIndex] != INVALID_CHILD_NODE)
			{
				if (!VerifyInsertionLeafState(leafIndex, aChildIndices[index]))
					continue;
				GetLeaf(lastIndices[(int)newIndex]).SetNextCellLeaf(leafIndex);
			}
			else
			{
				SQuadNode& rCurrentNode = GetNode(cFirstChildIndex + (int)newIndex);
				if (!VerifyInsertionLeafState(leafIndex, aChildIndices[index]))
					continue;
				rCurrentNode.SetIsLeaf(true);
				rCurrentNode.SetLeafIndex(leafIndex);
			}
			lastIndices[(int)newIndex] = leafIndex;
		}
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const EPosInsertionResult CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
PropagateLeavesToNewNodeFallback
(
  TChildIndices aChildIndices[TMaxCellElems + 1],
  const TVec2& crPos,
  const SQuadNodeData& crNodedata,
  const std::vector<TIndexType>& crCellLeaves,
  const TIndexType cNewLeafIndex
)
{
	//propagate all leaves to current node, called in case if max subdivision level is reached but still TMaxCellElems leaves share this node
	//prioritize leaves which have a pos in that particular cell, first check new leave and rejected it at once if pos is outside
	const bool cNewLeafIsInCell = IsPosInCell(crPos, crNodedata.sideLength, crNodedata.centerPos);
	//now iterate all cell leaf elements, as soon as the first non in this cell laying element is found, the new one gets inserted
#if defined(_DEBUG)
	TIndexType lastIndex;
	const SQuadNode& crNode = GetNode(crNodedata.nodeIndex);
	assert(!crNode.HasLeafNodes());  //should just recently been subdivided
	lastIndex = INVALID_CHILD_NODE;
#else
	TIndexType lastIndex = INVALID_CHILD_NODE;
#endif
	bool newLeafhasBeenInserted = cNewLeafIsInCell ? false : true;
	assert(TMaxCellElems == crCellLeaves.size());
	for (int index = 1; index <= (int)TMaxCellElems; ++index) //assign all leaves to all cells (according to crChildIndices)
	{
		TIndexType leafIndex = cNewLeafIndex;
		if (!newLeafhasBeenInserted &&
		    !IsPosInCell(GetLeaf(crCellLeaves[index - 1]).GetPos(), crNodedata.sideLength, crNodedata.centerPos) &&
		    VerifyInsertionLeafState(leafIndex, aChildIndices[0]))
		{
			//node is not situated in that cell, replace it by new leaf
			//insert new leaf
			if (lastIndex != INVALID_CHILD_NODE)
				GetLeaf(lastIndex).SetNextCellLeaf(leafIndex);
			else
			{
				SQuadNode& rCurrentNode = GetNode(crNodedata.nodeIndex);
				rCurrentNode.SetIsLeaf(true);
				rCurrentNode.SetLeafIndex(leafIndex);
			}
			lastIndex = leafIndex;
			newLeafhasBeenInserted = true;
		}
		else
		{
			leafIndex = crCellLeaves[index - 1];
			if (lastIndex != INVALID_CHILD_NODE)
			{
				if (!VerifyInsertionLeafState(leafIndex, aChildIndices[index]))
					continue;
				GetLeaf(lastIndex).SetNextCellLeaf(leafIndex);
			}
			else
			{
				SQuadNode& rCurrentNode = GetNode(crNodedata.nodeIndex);
				if (!VerifyInsertionLeafState(leafIndex, aChildIndices[index]))
					continue;
				rCurrentNode.SetIsLeaf(true);
				rCurrentNode.SetLeafIndex(leafIndex);
			}
			lastIndex = leafIndex;
		}
	}
	return cNewLeafIsInCell ? newLeafhasBeenInserted ? POS_SUCCESS : POS_FAILED : NON_POS_FAILED;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
ResetChildIndicesVec(TChildIndices aChildIndices[TMaxCellElems + 1])
{
	if (!TUseRadius)
		return;  //always constant at 1
	for (int i = 0; i <= TMaxCellElems; ++i)
		aChildIndices[i].count = 0;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::Reset()
{
	m_Leaves.clear();
	m_Nodes.clear();
	m_Center.x = m_Center.y = 0;
	m_SideLength = 0;
	m_DeletedLeaves.clear();
	m_MaxSubDivLevels = 0;
	m_MinSideLength = 0.f;
	m_UniqueLeaveCount = 0;
	m_MaxRadius = 0.f;
	m_CompressionFactor = 1.f;
	m_UniqueLeaveCount = 0;
#if defined(_DEBUG)
	m_DebugStats.Reset();
	m_IsInitialized = false;
#endif
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const EPosInsertionResult CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
InsertLeafWithSubdivisionRecurse
(
  const TVec2& crPos,
  const SQuadNodeData& crNodedata,
  const std::vector<TIndexType>& crCellLeaves,
  uint16 subDivLevel,
  const TIndexType cNewLeafIndex,
  const float cRadius,
  TChildIndices aChildIndices[TMaxCellElems + 1]
)
{
	if (m_MaxSubDivLevels <= subDivLevel++)   //we must not subdivide more than m_MaxSubDivLevels times
	{
		if (TUseRadius)
			return PropagateLeavesToNewNodeFallback(aChildIndices, crPos, crNodedata, crCellLeaves, cNewLeafIndex);
		else
		{
			assert(false);
			return POS_FAILED;  //should not happen in case of non TUseRadius since Verify should have taken care of that
		}
	}
	const TIndexType cFirstChildIndex = Subdivide(crNodedata.centerPos, GetNode(crNodedata.nodeIndex));
	//get subdivision data
	TVec2F childCenters[4];
	GetSubdivisionCenters(crNodedata.sideLength, crNodedata.centerPos, childCenters);
	SArraySelector<TUseRadius, bool> subdivisionInfo;
	for (int ind = 0; ind <= TMaxCellElems; ++ind)
		aChildIndices[ind].ResetForRecursion();
	ResetChildIndicesVec(aChildIndices);
	TEChildIndexExt posChildIndex;  //index where new node goes
	GatherChildInfo(crPos, crNodedata.centerPos, aChildIndices, crCellLeaves, cRadius, childCenters, crNodedata.sideLength * 0.5, subdivisionInfo, posChildIndex);
	//check if cell where node actually goes can be handled in PropagateLeavesToNewNode
	const bool cPosCellMasked = TUseRadius ? ((posChildIndex != NP_E /*pos outside all cells*/) && subdivisionInfo.elems[posChildIndex]) : subdivisionInfo.elems[0];
	if (cPosCellMasked)
	{
		assert((TIndexType)(cFirstChildIndex + (int)posChildIndex) < (TIndexType)m_Nodes.size());
		TChildIndices aChildIndicesCopy[TMaxCellElems + 1];
		for (int ind = 0; ind <= TMaxCellElems; ++ind)
			aChildIndicesCopy[ind].reinserted = aChildIndices[ind].reinserted;
		const EPosInsertionResult cSubRes = InsertLeafWithSubdivisionRecurse
		                                    (
		  crPos,
		  SQuadNodeData(cFirstChildIndex + posChildIndex, childCenters[(int)posChildIndex], crNodedata.sideLength * 0.5),
		  crCellLeaves,
		  subDivLevel,
		  cNewLeafIndex,
		  cRadius,
		  aChildIndicesCopy
		                                    );
		if (!TUseRadius)
			return cSubRes;
		else if (POS_FAILED == cSubRes)
			return POS_FAILED;

		for (int ind = 0; ind <= TMaxCellElems; ++ind)
			aChildIndices[ind].reinserted = aChildIndicesCopy[ind].reinserted;
	}
	if (TUseRadius || (!TUseRadius && !subdivisionInfo.elems[0])) //propagate all leaves to its respective cells
		PropagateLeavesToNewNode(aChildIndices, cFirstChildIndex, crCellLeaves, cNewLeafIndex, subdivisionInfo);
	if (!TUseRadius)
		return POS_SUCCESS;  //all has been done in case of not using radius
	else
	{
		int failCount = 0;
		for (int i = 0; i < (TUseRadius ? 4 : 1); ++i)
		{
			//recurse to all children
			if (i != posChildIndex /*handled already*/ && subdivisionInfo.elems[i])//masked and therefore needs recursion
			{
				const TEChildIndex cNewIndex = (TEChildIndex)i;
				assert((TIndexType)(cFirstChildIndex + (int)cNewIndex) < (TIndexType)m_Nodes.size());
				const EPosInsertionResult cSubRes = InsertLeafWithSubdivisionRecurse
				                                    (
				  crPos,
				  SQuadNodeData(cFirstChildIndex + cNewIndex, childCenters[(int)cNewIndex], crNodedata.sideLength * 0.5),
				  crCellLeaves,
				  subDivLevel,
				  cNewLeafIndex,
				  cRadius,
				  aChildIndices
				                                    );
				assert(POS_FAILED != cSubRes);
				failCount += (NON_POS_FAILED == cSubRes) ? 1 : 0;
			}
		}
		//check whether all child insertions have been failed or just some, leaf was nowhere inserted
		return (failCount > 0) ? NON_POS_FAILED : POS_SUCCESS;
	}
	return POS_SUCCESS;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const EInsertionResult CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
InsertLeaf(const TVec2& crPos, const TLeafContent& crContent, const float cRadius, const bool cVerify)
{
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
	m_DebugStats.Reset();
#endif
	//check pos being inside quadtree area
	if (abs(crPos.x - m_Center.x) > m_SideLength / 2.0f || abs(crPos.y - m_Center.y) > m_SideLength / 2.0f ||
	    crPos.x >= m_Center.x + m_SideLength / 2.0f || crPos.y >= m_Center.y + m_SideLength / 2.0f)
		return LEAVE_OUTSIDE_QUADTREE;
#if defined(_DEBUG)
	m_DebugStats.leafArrayAccesses++;
#endif
	if (m_Leaves.size() >= MaxElems())
		return TOO_MANY_LEAVES;
	//find out leaf where it belongs and add it there if less than TMaxCellElems leaves are contained there already
	TNodeDataVec nodeDataVec;
	SQuadNodeData nodeData;
	assert(m_Nodes.size() > 0);
	TTravRecVecType traversedNodes;  //need for function call
	uint16 subDivLevel = 0;
	const TRadiusType cLeafRadius = TUseRadius ? (CompressRadius(std::min(m_MaxRadius, cRadius))) : 0;
	const float cEffectiveRadius = TUseRadius ? GetRadius(cLeafRadius) : 0;
	if (TUseRadius)
	{
		nodeDataVec.reserve(20);
		assert(m_MaxRadius > cRadius);
		GatherAllRadiusSharingNodes(nodeDataVec, SQuadNodeData(0, m_Center, (float)m_SideLength), crPos, cEffectiveRadius);
	}
	//we need the node index for the exact position insertion
	RetrieveDeepestNode(crPos, (float)m_SideLength, m_Center, nodeData, 0, traversedNodes, subDivLevel);
	//iterate all nodes where the new leaf has to be inserted
	//we need this because insertion for TUseRadius has 2 stages (1st run only inserts at the exact position, if this fails, nothing is inserted)
	bool firstRun = true;
	for (int stage = 0; stage < (TUseRadius ? 2 : 1); ++stage)
	{
		for (TIndexType nodeInd = 0; nodeInd < (TUseRadius ? nodeDataVec.size() : 1); ++nodeInd)
		{
			const SQuadNodeData& crNodeData = TUseRadius ? nodeDataVec[nodeInd].first : nodeData;
			//get sub div level, specific for every node, does not matter in non radius mode
			subDivLevel = TUseRadius ? nodeDataVec[nodeInd].second : subDivLevel;
			const TIndexType cCurIndex = crNodeData.nodeIndex;
			if (TUseRadius)
			{
				if (firstRun)
				{
					if (nodeData.nodeIndex != cCurIndex)
						continue;   //if radius is to be used, only process node in the first stage
				}
				else if (nodeData.nodeIndex == cCurIndex)
					continue;     //if radius is to be used, only process node in the first stage
			}
			const TIndexType cNewLeafIndex = (TIndexType)m_Leaves.size();
#if defined(_DEBUG)
			m_DebugStats.leafArrayAccesses++;
#endif
			//in case of radius, insert a copy of it for each shared node for simplicity and fast access
			TLeaf leaf(crContent, crPos, cLeafRadius);
			//1st case: no child nodes and it is no leaf -> turn it into a leaf and insert new leaf
			SQuadNode& rNode = GetNode(crNodeData.nodeIndex);
			if (!rNode.HasLeafNodes())
			{
				m_Leaves.push_back(leaf);
				rNode.SetIsLeaf(true);
				rNode.SetLeafIndex(cNewLeafIndex);
			}
			else
			{
				std::vector<TIndexType> cellLeaves;
				cellLeaves.reserve(TMaxCellElems);
				const TIndexType cCellCount = GetCellLeafCount(rNode, cellLeaves);
				//dont insert leaves with the position with a distance less than the pos type constraint, also checks for position equality
				const EInsertionResult cVerficationRes = (cVerify && firstRun) ? VerifyLeafPos(cellLeaves, crPos, crNodeData, subDivLevel) : SUCCESS;
				if (cVerficationRes != SUCCESS)
					return cVerficationRes;
				if (cVerify && !firstRun && (cellLeaves.size() == TMaxCellElems))
				{
					//this is taken care of in VerifyLeafPos too
					if (subDivLevel >= m_MaxSubDivLevels && cellLeaves.size() >= TMaxCellElems) //cannot subdivide anymore
						continue;
				}
				m_Leaves.push_back(leaf);  //leaf can be inserted
				assert(cCellCount > 0);
				if (cCellCount < TMaxCellElems)
				{
					assert(cellLeaves.size() == cCellCount);
					//2nd case: is leaf node and already contained leaf count is less than TMaxCellElems -> insert new leaf and link to previous one
					GetLeaf(cellLeaves[cCellCount - 1]).SetNextCellLeaf(cNewLeafIndex);
				}
				else
				{
					//3rd case: is leaf node and already contained leaf count TMaxCellElems ->
					//turn back into node, subdivide node and insert new leaf and all cell leaves accordingly
					assert(cCellCount == TMaxCellElems);
					rNode.childNodeIndex = 0;   //remove leaf index
					TChildIndices childIndices[TMaxCellElems + 1];//static to avoid reallocations
					for (int ind = 0; ind <= TMaxCellElems; ++ind)
						childIndices[ind].CompleteReset();
					if (POS_FAILED == InsertLeafWithSubdivisionRecurse(crPos, crNodeData, cellLeaves, subDivLevel, cNewLeafIndex, cEffectiveRadius, childIndices))
					{
						if (firstRun)
						{
							m_Leaves.pop_back();  //remove last inserted element
							return NOT_ENOUGH_SUBDIV_LEVELS;
						}
						else
						{
							if (TUseRadius)
								assert(false);  //must not come here in the 2nd pass
							continue;
						}
					}
				}
			}
			if (firstRun)
				break;  //we only insert one node in  the first run
		}
		firstRun = false;
	}
	//when we get here, the node has successfully been inserted
	m_UniqueLeaveCount++;
	return SUCCESS;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TNodeConstraint>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetClosestContents
(
  const TVec2& crPos,
  const TNodeConstraint cNodeConstraint,
  std::vector<TContentRetrievalType>& rContents,
  const bool cSort
) const
{
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
#endif
	rContents.resize(0);
	//construct to avoid wrong usage since any atomic type can be converted to float or TIndexType
	TSearchContainer cont
	(
	  rContents,
	  (typename SClosestContentsChecker<TNodeConstraint, TNodeConstraint>::Type)(cNodeConstraint),
	  cSort
	);
	GetClosestContentsContainer(crPos, cont);
	cont.FillVector((typename SClosestContentsChecker<TNodeConstraint, TNodeConstraint>::Type)cNodeConstraint);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetClosestContentsContainer
(
  const TVec2& crPos,
  TSearchContainer& rSortedContents
) const
{
#if defined(_DEBUG)
	m_DebugStats.Reset();
#endif
	//idea: find node the pos is in and retrieve the closest contents from this cell
	//			traverse the tree back checking all cells with the min distance and the largest from the so far collected contents
	//			traverse into the leaves if distance to one cell is less
	SQuadNodeData nodeData;
	TTravRecVecType traversedNodes;
	uint16 subDivLevel = 0;
	RetrieveDeepestNode(crPos, (float)m_SideLength, m_Center, nodeData, 0, traversedNodes, subDivLevel);    //find node
	const SQuadNode& crNode = GetNode(nodeData.nodeIndex);
	//gather all nodes if it is a leaf, not necessary though since tree might not be optimized due to removed elems
	if (crNode.HasLeafNodes())
	{
		const TLeaf& crLeaf = GetLeaf(crNode.GetLeafIndex());
		GetClosestLeafContents(crLeaf, crPos, nodeData.sideLength, nodeData.centerPos, rSortedContents);
	}

	//now traverse all traversed leaves and check min distance to cell
	const uint32 cCount = traversedNodes.size();
	for (int i = cCount - 1; i >= 0; --i)
	{
		//traverse all nodes from bottom to the top, invoke with ::Type cast to be able to call for uint32 as index type too
		GetClosestNodeContents(traversedNodes[i], crPos, rSortedContents);
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetClosestNodeContents
(
  const TTravRecType& crStartNodeRec,
  const TVec2& crPos,
  TSearchContainer& rSortedContents
) const
{
	//idea: start at crStartNodeRec->first.nodeIndex node and check all cells (but not crStartNodeRec->second), if min dist to it is less than largest
	//in rSortedContents contained distance, traverse this node down and collect the contents (applying same criteria)
	//first check all 3 cells
	const SQuadNodeData& crQuadNodeData = crStartNodeRec.first;
	const SQuadNode& crNode = GetNode(crQuadNodeData.nodeIndex);
	assert(!crNode.HasLeafNodes());
	if (!crNode.HasChildNodes())
		return;   //nothing to do here
	//retrieve child data
	TVec2F childCenters[4];
	GetSubdivisionCenters(crQuadNodeData.sideLength, crQuadNodeData.centerPos, childCenters);
	for (int i = 0; i < 4; ++i)
		if (i != crStartNodeRec.second && rSortedContents.CheckInsertCriteria(GetMinDistToQuadSq(crQuadNodeData.sideLength / 2.0f, childCenters[i], crPos)))
			GetClosestContentsDistRecurse(crNode.childNodeIndex + i, childCenters[i], crQuadNodeData.sideLength / 2.0f, crPos, rSortedContents);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetNodeRayIntersectionContents
(
  const TTravRecType& crStartNodeRec,
  std::vector<const TLeafContent*>& rContents,
  const TVec2F& crPos,
  const TVec2F& crDir,
  const float cRayLength,
  const uint32 cMaxCount,
  uint32& rCurCount
) const
{
	//idea: start at crStartNodeRec->first.nodeIndex node and check all cells (but not crStartNodeRec->second), if min dist to it is less than largest
	//in rSortedContents contained distance, traverse this node down and collect the contents (applying same criteria)
	//first check all 3 cells
	const SQuadNodeData& crQuadNodeData = crStartNodeRec.first;
	const SQuadNode& crNode = GetNode(crQuadNodeData.nodeIndex);
	assert(!crNode.HasLeafNodes());
	if (!crNode.HasChildNodes())
		return;   //nothing to do here
	//retrieve child data
	TVec2F childCenters[4];
	GetSubdivisionCenters(crQuadNodeData.sideLength, crQuadNodeData.centerPos, childCenters);
	for (int i = 0; i < 4; ++i)
		if (i != crStartNodeRec.second)
			if (GatherAllIntersectingNodes(rContents, SQuadNodeData(crNode.childNodeIndex + i, childCenters[i], crQuadNodeData.sideLength / 2.0f), crPos, crDir, cRayLength, cMaxCount, rCurCount))
				break;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetClosestContentsDistRecurse
(
  const TIndexType cNodeIndex,
  const TVec2F& crCenterPos,
  const float cSideLength,
  const TVec2& crPos,
  TSearchContainer& rSortedContents
) const
{
	const SQuadNode& crNode = GetNode(cNodeIndex);
	if (crNode.HasLeafNodes())
	{
		const TLeaf& crLeaf = GetLeaf(crNode.GetLeafIndex());
		GetClosestLeafContents(crLeaf, crPos, cSideLength, crCenterPos, rSortedContents);
		return;
	}
	if (crNode.HasChildNodes())
	{
		TVec2F childCenters[4];
		GetSubdivisionCenters(cSideLength, crCenterPos, childCenters);
		for (int i = 0; i < 4; ++i)
			if (rSortedContents.CheckInsertCriteria(GetMinDistToQuadSq(cSideLength / 2.0f, childCenters[i], crPos)))
				GetClosestContentsDistRecurse(crNode.childNodeIndex + i, childCenters[i], cSideLength / 2.0f, crPos, rSortedContents);
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
AddLeafContent(const SContentSort& crContToAdd, TSearchContainer& rSortedContents) const
{
	//idea: add element if it has a smaller distance than the last one (saves work)
	//keep rSortedContents within cNodeCount
	if (rSortedContents.CheckInsertCriteria(crContToAdd.distSq))
		rSortedContents.Insert(crContToAdd, crContToAdd.distSq);  //add new one and remove last one
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetClosestLeafContents
(
  const TLeaf& crLeaf,
  const TVec2& crPos,
  const float cSideLength,
  const TVec2F& crCenterPos,
  TSearchContainer& rSortedContents
) const
{
	//idea: add all elements of this leaf as long as it fulfills the cNodeConstraint (currently radius or max count)
	//after that, only add if element has smaller distance than the last set element
	const float cRadius = GetRadiusFromLeaf(crLeaf);
	if (IsPosInCell(crLeaf.GetPos(), cSideLength, crCenterPos, cRadius))
	{
		const float cDist = std::max(0.f, crLeaf.GetPos().GetDist(crPos) - cRadius);
		AddLeafContent(SContentSort(
		                 (TLeafContent*)crLeaf.GetContentPointer(),
		                 crLeaf.GetPos(),
		                 cDist * cDist /*need square distance here*/
		                 ), rSortedContents);
	}
	TIndexType cellLeafIndex = crLeaf.GetNextCellLeaf();
	for (int i = 0; i < TMaxCellElems - 1; ++i)
	{
		if (cellLeafIndex == TLeaf::INVALID_LEAF_INDEX)
			break;
		const TLeaf& crNexLeaf = GetLeaf(cellLeafIndex);
		cellLeafIndex = crNexLeaf.GetNextCellLeaf();
		const float cNextRadius = GetRadiusFromLeaf(crNexLeaf);
		if (IsPosInCell(crNexLeaf.GetPos(), cSideLength, crCenterPos, cNextRadius))
		{
			const float cDist = std::max(0.f, crNexLeaf.GetPos().GetDist(crPos) - cNextRadius);
			AddLeafContent(SContentSort(
			                 (TLeafContent*)crNexLeaf.GetContentPointer(),
			                 crNexLeaf.GetPos(),
			                 cDist * cDist /*need square distance here*/
			                 ),
			               rSortedContents);
		}
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
SerializeTree(std::vector<uint8>& rStream, SerializeContentFunction pSerializeFnct, SerializeContentSizeFunction pSizeFnct, const bool cOptimizeBefore)
{
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
	m_DebugStats.Reset();
#endif
	rStream.resize(0);
	if (cOptimizeBefore)
		OptimizeTree();   //first optimize to not store unnecessary data
	SHeader header;
	header.uniqueLeaveCount = m_UniqueLeaveCount;
	header.leafCount = (TIndexType)m_Leaves.size();
	header.nodeCount = (TIndexType)m_Nodes.size();
	header.centerPos = m_Center;
	header.sideLength = m_SideLength;
	header.maxSubDivLevels = m_MaxSubDivLevels;
	//calculate size of stream
	const size_t cContentSerializationSize = pSizeFnct();
	const size_t cLeafSerializationSize = TLeaf::GetSerializationSize(cContentSerializationSize);
	const size_t cStreamSize = sizeof(header) + sizeof(SQuadNode) * m_Nodes.size() + cLeafSerializationSize * m_Leaves.size();
	header.fullStreamSize = (uint32)cStreamSize;
	header.minSideLength = m_MinSideLength;
	rStream.resize(cStreamSize);
	uint8* pCurStreamPos = &rStream[0];  //vector is not gonna be reallocated, pointer usage is safe here
	//first copy header
	memcpy(pCurStreamPos, &header, sizeof(header));
	pCurStreamPos += sizeof(header);
	//copy nodes
	memcpy(pCurStreamPos, &m_Nodes[0], sizeof(SQuadNode) * m_Nodes.size());
	pCurStreamPos += sizeof(SQuadNode) * m_Nodes.size();
	//copy leaves
	const typename TLeafVect::const_iterator cLeafEnd = m_Leaves.end();
	for (typename TLeafVect::const_iterator iter = m_Leaves.begin(); iter != cLeafEnd; ++iter)
		iter->Serialize(pSerializeFnct, cContentSerializationSize, pCurStreamPos);
	assert((size_t)pCurStreamPos - (size_t)&rStream[0] == cStreamSize);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
ConstructTreeFromStream(const uint8* cpStream, const uint32 cStreamSizeToDecode, DeSerializeContentFunction pDeSerializeFnct, SerializeContentSizeFunction pSizeFnct, uint8*& newStreamPos)
{
#if defined(_DEBUG)
	m_DebugStats.Reset();
#endif
	Reset();
	assert(cpStream);
	//first read header byte
	uint32 cHeaderSize = *(uint32*)cpStream;
	SwapEndian(cHeaderSize);
	assert(sizeof(SHeader) == cHeaderSize);
	if (sizeof(SHeader) != cHeaderSize)
		return false;   //dont match
	assert(sizeof(SHeader) < cStreamSizeToDecode);
	//read header
	SHeader header;
	memcpy(&header, cpStream, cHeaderSize);
	header.SwapEndianess();
	uint8* pCurStreamPos = const_cast<uint8*>(&cpStream[cHeaderSize]);  //point to first byte behind header
	//reconstruct data from header
	m_Center = header.centerPos;
	m_UniqueLeaveCount = header.uniqueLeaveCount;
	m_SideLength = header.sideLength;
	m_MaxSubDivLevels = header.maxSubDivLevels;
	m_MinSideLength = header.minSideLength;
	m_DeletedLeaves.clear();
	//read nodes
	m_Nodes.resize(header.nodeCount);
	memcpy(&m_Nodes[0], pCurStreamPos, sizeof(SQuadNode) * header.nodeCount);
	for (int i = 0; i < header.nodeCount; ++i)
		m_Nodes[i].SwapEndianess();
	pCurStreamPos += sizeof(SQuadNode) * header.nodeCount;
	if (TUseRadius)
		InitializeRadiusCompression();
	//read leaves
	m_Leaves.clear();
	m_Leaves.reserve(header.leafCount);
	const size_t cContentSerializationSize = pSizeFnct();
	for (int i = 0; i < header.leafCount; ++i)
	{
		TLeaf leaf;
		TLeafContent cont;
		if (!pDeSerializeFnct(cont, pCurStreamPos))
			return false;
		leaf.SetContent(cont);
		TVec2 pos;
		memcpy(&pos, pCurStreamPos + cContentSerializationSize, sizeof(TVec2));
		pos.SwapEndianess();
		leaf.SetPos(pos);
		assert(m_Leaves.size() < MaxElems());  //shouldnt happen here
		memcpy(&leaf.nextLeafIndex, pCurStreamPos + cContentSerializationSize + sizeof(TVec2), sizeof(TIndexType));
		SwapEndian(leaf.nextLeafIndex);
		m_Leaves.push_back(leaf);
		pCurStreamPos += TLeaf::GetSerializationSize(cContentSerializationSize);
	}
	assert((size_t)pCurStreamPos - (size_t)&cpStream[0] <= cStreamSizeToDecode);
	SetMaxSubdivLevels(m_MaxSubDivLevels);
#if defined(_DEBUG)
	m_IsInitialized = true;
#endif
	newStreamPos = pCurStreamPos;
	return true;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::Draw(const char* cpFileName)
{
#if defined(_DEBUG)
	assert(m_IsInitialized == true);
#endif
	OptimizeTree();   //optimize tree before storing inconsistent node-leaf information
	//compute colours and size, starts with 1 pixel at lowest level
	//max size is 2048x2048 -> max 11 subdivision levels
	const uint16 cMaxLevel = min(m_MaxSubDivLevels, (uint16)11);
	const uint32 cSize = (uint32)pow(2.f, (int)cMaxLevel);
	uint32* pBitmap = new uint32[cSize * cSize];
	assert(pBitmap);
	memset(pBitmap, 0, cSize * cSize * sizeof(uint32));
	std::vector<uint32> RGBColours(cMaxLevel + 1);
	//calc RGB colour
	const uint32 cMaxYellowLevel = cMaxLevel / 2;
	for (int i = 0; i <= cMaxYellowLevel; ++i)
	{
		const float cInterpolVal = (float)i / (float)(cMaxYellowLevel);
		RGBColours[i] = LerpColour(cInterpolVal, Vec3(255, 0, 0), Vec3(0, 255, 0));
	}
	for (int i = cMaxYellowLevel + 1; i <= cMaxLevel; ++i)
	{
		const float cInterpolVal = (float)(i - cMaxYellowLevel) / (float)(cMaxLevel - cMaxYellowLevel);
		RGBColours[i] = LerpColour(cInterpolVal, Vec3(0, 255, 0), Vec3(0, 0, 255));
	}
	std::vector<TIndexType> cellLeaves;
	//now iterate each pixel, map to a position and retrieve the leaf information with subdivision level and number of contained leaves
	const float cPosScale = (float)m_SideLength / (float)cSize;
	const float cStartPosX((float)m_Center.x - (float)m_SideLength * 0.5 + 0.5f * cPosScale);   //set to left upper plus half texel offset
	const float cStartPosY((float)m_Center.y - (float)m_SideLength * 0.5 + 0.5f * cPosScale);
	for (uint32 y = 0; y < cSize; ++y)
		for (uint32 x = 0; x < cSize; ++x)
		{
			const TVec2 crPos((TPosType)(cStartPosX + (float)x * cPosScale), (TPosType)(cStartPosY + (float)y * cPosScale));
			SQuadNodeData nodeData;
			TTravRecVecType traversedNodes;  //need for function call
			uint16 subDivLevel = 0;
			RetrieveDeepestNode(crPos, (float)m_SideLength, m_Center, nodeData, 0, traversedNodes, subDivLevel);
			assert(subDivLevel <= m_MaxSubDivLevels);
			const SQuadNode& crNode = GetNode(nodeData.nodeIndex);
			const TIndexType cCellLeafCount = GetCellLeafCount(crNode, cellLeaves);
			pBitmap[y * cSize + x] = AddAlpha(RGBColours[min(subDivLevel, cMaxLevel)], (float)cCellLeafCount / TMaxCellElems);
		}
	SaveTGA32(cpFileName, pBitmap, cSize);
	delete[] pBitmap;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
IsPosInCell(const TVec2& crPos, const float cSideLength, const TVec2F& crCenter) const
{
	if (TUseRadius)
	{
		//get non rotated pos and check x/y extend against sidelength
		const TVec2F cCenterPosNoRot = crCenter;                    //center pos set in constructor, so this is safe and good here
		const TVec2F crNonRotPos = (TVec2F)crPos - cCenterPosNoRot; //place around 0,0
		const float cHalfSideLength = cSideLength * 0.5f;
		bool xCond = (fabs(crNonRotPos.x) > cHalfSideLength);
		bool yCond = (fabs(crNonRotPos.y) > cHalfSideLength);
		//need special case here since it should only go to a single cell if it is equal right on the border
		if (fabs(crNonRotPos.x) == cHalfSideLength)
			xCond = (crNonRotPos.x >= 0);
		if (fabs(crNonRotPos.y) == cHalfSideLength)
			yCond = (crNonRotPos.y >= 0);
		if (xCond || yCond)
			return false;
	}
	return true;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
IsPosInCell(const TVec2& crPos, const float cSideLength, const TVec2F& crCenter, const float cRadius) const
{
	if (TUseRadius)
	{
		//get non rotated pos and check x/y extend against sidelength
		const TVec2F cCenterPosNoRot = crCenter;
		const TVec2F crNonRotPos = (TVec2F)crPos - cCenterPosNoRot;  //place around 0,0
		const float cHalfSideLength = cSideLength * 0.5f;
		if (abs(crNonRotPos.x) >= cHalfSideLength || abs(crNonRotPos.y) >= cHalfSideLength)
		{
			//pos outside cell, check with radius
			const double cLength = sqrtf((float)(crNonRotPos.x * crNonRotPos.x + crNonRotPos.y * crNonRotPos.y));
			if (cLength < cRadius)
				return true;  //distance to ray pos within radius
			const double cInvLength = 1.0f / cLength;
			const TVec2D cDir((double)crNonRotPos.x * cInvLength, (double)crNonRotPos.y * cInvLength);  //direction vector
			const TVec2F cNewTestPos((float)(cDir.x * (cLength - cRadius)), (float)(cDir.y * (cLength - cRadius)));

			bool xCond = fabs(cNewTestPos.x) > cHalfSideLength;
			bool yCond = fabs(cNewTestPos.y) > cHalfSideLength;
			//need special case here since it should only go to a single cell if it is equal right on the border
			if (fabs(cNewTestPos.x) == cHalfSideLength)
				xCond = (cNewTestPos.x >= 0);
			if (fabs(cNewTestPos.y) == cHalfSideLength)
				yCond = (cNewTestPos.y >= 0);
			if (xCond || yCond)
				return false;
		}
	}
	return true;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetRadiusNodeCellsFromTraversal
(
  TNeighborCellsType& rCells,
  const TTravRecType& crStartNodeRec,
  const TVec2F& crPos,
  const float cRadiusSq
) const
{
	//idea: start at crStartNodeRec->first.nodeIndex node and check all cells (but not crStartNodeRec->second), if min dist to it is less than largest
	//in rSortedContents contained distance, traverse this node down and collect cells within the circle
	//first check all 3 cells
	const SQuadNodeData& crQuadNodeData = crStartNodeRec.first;
	const SQuadNode& crNode = GetNode(crQuadNodeData.nodeIndex);
	assert(!crNode.HasLeafNodes());
	if (!crNode.HasChildNodes())
		return;   //nothing to do here
	//retrieve child data
	TVec2F childCenters[4];
	GetSubdivisionCenters(crQuadNodeData.sideLength, crQuadNodeData.centerPos, childCenters);
	for (int i = 0; i < 4; ++i)
	{
		if (i != crStartNodeRec.second && cRadiusSq > GetMinDistToQuadSq(crQuadNodeData.sideLength / 2.0f, childCenters[i], crPos))
		{
			const SQuadNode& crChildNode = GetNode(crNode.childNodeIndex + i);
			if (crChildNode.HasLeafNodes())
			{
				rCells.push_back(SQuadNodeData(crNode.childNodeIndex + i, childCenters[i], crQuadNodeData.sideLength / 2.0f));
				continue;
			}
			if (crChildNode.childNodeIndex != 0) //check here to save function call
				GetRadiusNodeCellsRecurse(rCells, crChildNode, childCenters[i], crQuadNodeData.sideLength / 2.0f, crPos, cRadiusSq);
		}
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetRadiusNodeCellsRecurse
(
  TNeighborCellsType& rCells,
  const SQuadNode& crNode,
  const TVec2F& crCenterPos,
  const float cSideLength,
  const TVec2F& crPos,
  const float cRadiusSq
) const
{
	assert(crNode.HasChildNodes());
	TVec2F childCenters[4];
	GetSubdivisionCenters(cSideLength, crCenterPos, childCenters);

	const float cHalvedSideLen = cSideLength / 2.0f;

	//unroll code, need to be fast
	const SQuadNode& crNode0 = GetNode(crNode.childNodeIndex);
	const SQuadNode& crNode1 = GetNode(crNode.childNodeIndex + 1);
	const SQuadNode& crNode2 = GetNode(crNode.childNodeIndex + 2);
	const SQuadNode& crNode3 = GetNode(crNode.childNodeIndex + 3);
	const bool cHasChildren0 = (crNode0.childNodeIndex != 0);
	const bool cHasChildren1 = (crNode1.childNodeIndex != 0);
	const bool cHasChildren2 = (crNode2.childNodeIndex != 0);
	const bool cHasChildren3 = (crNode3.childNodeIndex != 0);
	float distSq[4] =
	{
		GetMinDistToQuadSq(cHalvedSideLen, childCenters[0], crPos),
		GetMinDistToQuadSq(cHalvedSideLen, childCenters[1], crPos),
		GetMinDistToQuadSq(cHalvedSideLen, childCenters[2], crPos),
		GetMinDistToQuadSq(cHalvedSideLen, childCenters[3], crPos)
	};

	if (cHasChildren0 && cRadiusSq > distSq[0])
	{
		if (crNode0.HasLeafNodes())
			rCells.push_back(SQuadNodeData(crNode.childNodeIndex, childCenters[0], cHalvedSideLen));
		else
			GetRadiusNodeCellsRecurse(rCells, crNode0, childCenters[0], cHalvedSideLen, crPos, cRadiusSq);
	}
	if (cHasChildren1 && cRadiusSq > distSq[1])
	{
		if (crNode1.HasLeafNodes())
			rCells.push_back(SQuadNodeData(crNode.childNodeIndex + 1, childCenters[1], cHalvedSideLen));
		else
			GetRadiusNodeCellsRecurse(rCells, crNode1, childCenters[1], cHalvedSideLen, crPos, cRadiusSq);
	}
	if (cHasChildren2 && cRadiusSq > distSq[2])
	{
		if (crNode2.HasLeafNodes())
			rCells.push_back(SQuadNodeData(crNode.childNodeIndex + 2, childCenters[2], cHalvedSideLen));
		else
			GetRadiusNodeCellsRecurse(rCells, crNode2, childCenters[2], cHalvedSideLen, crPos, cRadiusSq);
	}
	if (cHasChildren3 && cRadiusSq > distSq[3])
	{
		if (crNode3.HasLeafNodes())
			rCells.push_back(SQuadNodeData(crNode.childNodeIndex + 3, childCenters[3], cHalvedSideLen));
		else
			GetRadiusNodeCellsRecurse(rCells, crNode3, childCenters[3], cHalvedSideLen, crPos, cRadiusSq);
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::PropagateNeighbourLeaves
(
  const TNeighborCellsType& crCells,
  SInterpolRetrievalType* pLeaves,
  const TVec2F& crPos,
  const float cMaxDistSq
) const
{
	const uint32 cCellSize = crCells.size();
	SInterpolNodeData nodeData;
	for (int i = 0; i < cCellSize; ++i)
	{
		const SQuadNodeData& crNodeData = crCells[i];
		const SQuadNode& crNode = GetNode(crNodeData.nodeIndex);
		assert(crNode.HasLeafNodes());
		FillInterpolationNodeData(crNode, crPos, nodeData, cMaxDistSq);
		for (int e = 0; e < nodeData.count; ++e)
		{
			const SInterpolRetrievalTypeHelper& crNodeRetrieval = nodeData.data[e];
			const float cNodeRetrievalDist = crNodeRetrieval.distSq;
			if (cMaxDistSq > cNodeRetrievalDist)
			{
				SInterpolRetrievalType& rLeaf = pLeaves[crNodeRetrieval.sliceIndex];
				if (!rLeaf.cpCont || rLeaf.distSq > cNodeRetrievalDist)
					rLeaf = crNodeRetrieval;
				else
				{
					SInterpolRetrievalType& rNextLeaf = pLeaves[crNodeRetrieval.nextSliceIndex];
					if (!rNextLeaf.cpCont /* || rLeaf.distSq > cNodeRetrievalDist*/)
						rNextLeaf = crNodeRetrieval;
				}
			}
		}
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::EConsistencyRes
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::IsConsistent()
{
	//check leaf indices from nodes
	const uint32 cLeafCount = (uint32)m_Leaves.size();
	const typename TNodeVec::const_iterator cNodeEnd = m_Nodes.end();
	for (typename TNodeVec::const_iterator iter = m_Nodes.begin(); iter != cNodeEnd; ++iter)
	{
		const SQuadNode& crNode = *iter;
		const TIndexType cLeafIndex = (crNode.HasLeafNodes()) ? crNode.GetLeafIndex() : 0;
		if (cLeafIndex >= cLeafCount)
			return CR_INCONSISTENT;  //broken, cannot fix
	}
	bool fixed = false;
	//check leaf indices from leaves
	const typename TLeafVect::iterator cLeafEnd = m_Leaves.end();
	for (typename TLeafVect::iterator iter = m_Leaves.begin(); iter != cLeafEnd; ++iter)
	{
		TLeaf& rLeaf = *iter;
		const TIndexType cLeafIndex = rLeaf.GetNextCellLeaf();
		if (cLeafIndex != TLeaf::INVALID_LEAF_INDEX && cLeafIndex >= cLeafCount)
		{
			//broken, fix it temporarily by marking it as no more leaves
			rLeaf.SetNextCellLeaf(0, false);
			fixed = true;
		}
	}
	return fixed ? CR_FIXED_INCONSISTENCY : CR_CONSISTENT;
}

#pragma warning(disable:4715)
template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const uint8 CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetSliceIndex(const float cDirX, const float cDirY, const float cDistSq, uint8& rNextIndex) const
{
	static const float scLookupScale = (float)(scLookupCount / 2) - 0.00001f;
	const float cS = sqrtf(cDistSq);
	//determine slice index
	if (cS < 0.00001f)
	{
		rNextIndex = 1;
		return 0;
	}
	const std::pair<uint8, uint8>& crSlicesIndices = m_SliceLookupTable[(uint32)(scLookupScale * (cDirY / cS + 1.f))][(cDirX >= 0.f) ? 0 : 1];
	rNextIndex = crSlicesIndices.second;
	return crSlicesIndices.first;
}
#pragma warning(default:4715)

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
FillInterpolationNodeData(const SQuadNode& crNode, const TVec2F& crPos, SInterpolNodeData& rNodeData, const float cMaxDistSq) const
{
	rNodeData.count = 0;
	TIndexType leafIndex = (crNode.HasLeafNodes()) ? crNode.GetLeafIndex() : TLeaf::INVALID_LEAF_INDEX;
	if (leafIndex != TLeaf::INVALID_LEAF_INDEX)
	{
		for (;; )
		{
			const TLeaf& crLeaf = GetLeaf(leafIndex);
			//determine in which quadrant it actually lies in and if no cell is there yet, set this leaf there
			SInterpolRetrievalTypeHelper& rData = rNodeData.data[rNodeData.count++];
			rData.pos = crLeaf.GetPos();
			rData.cpCont = crLeaf.GetContentPointer();

			const float cDirX = (float)rData.pos.x - (float)crPos.x;
			const float cDirY = (float)rData.pos.y - (float)crPos.y;
			rData.distSq = (cDirX * cDirX + cDirY * cDirY);

			rData.sliceIndex = (rData.distSq <= cMaxDistSq) ?
			                   GetSliceIndex(cDirX, cDirY, rData.distSq, rData.nextSliceIndex) : 0;//dont query index if outside radius
			leafIndex = crLeaf.GetNextCellLeaf();
			if (leafIndex == TLeaf::INVALID_LEAF_INDEX)
				break;
		}
	}
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetInterpolationNodes(TInitialInterpolRetrievalType pLeaves, const TVec2F& crPos, const float cMaxDistSq) const
{
	//crPos is supposed to be in non rotated space
	assert(!TUseRadius);  //not supported yet
	TInitialInterpolRetrievalType initialLeaves;
	for (int i = 0; i < scInitalSliceGranularity; ++i)
		initialLeaves[i].cpCont = NULL;
	const TVec2F& crOffsetPos = crPos;
	SQuadNodeData nodeData;
	TTravRecVecType traversedNodes;
	uint16 subDivLevel = 0;
	RetrieveDeepestNode(crPos, (float)m_SideLength, m_Center, nodeData, 0, traversedNodes, subDivLevel);
	//now gather the elements within that cell and use them as interpolation nodes if they fit into the closest quarter area
	const SQuadNode& crParentNode = GetNode(nodeData.nodeIndex);
	TIndexType leafIndex = (crParentNode.HasLeafNodes()) ? crParentNode.GetLeafIndex() : TLeaf::INVALID_LEAF_INDEX;
	if (leafIndex != TLeaf::INVALID_LEAF_INDEX)
	{
		for (;; )
		{
			const TLeaf& crLeaf = GetLeaf(leafIndex);
			//determine in which slice it actually lies in and if no cell is there yet, set this leaf there
			const TVec2& crLeafPos = crLeaf.GetPos();

			const float cDirX = (float)crLeafPos.x - (float)crOffsetPos.x;
			const float cDirY = (float)crLeafPos.y - (float)crOffsetPos.y;
			const float cDistSq = (cDirX * cDirX + cDirY * cDirY);

			if (cMaxDistSq >= cDistSq)
			{
				uint8 nextSliceIndex;
				const uint8 cSliceIndex = GetSliceIndex(cDirX, cDirY, cDistSq, nextSliceIndex);
				assert(cSliceIndex < scInitalSliceGranularity);
				SInterpolRetrievalType& rLeaf = initialLeaves[cSliceIndex];
				if (!rLeaf.cpCont)
				{
					rLeaf.pos = crLeafPos;
					rLeaf.cpCont = crLeaf.GetContentPointer();
					rLeaf.distSq = cDistSq;
				}
				else
				{
					//check whether this leaf is closer than the current one
					if (cMaxDistSq >= cDistSq)
					{
						if (cDistSq < rLeaf.distSq)
						{
							//replace element
							rLeaf.pos = crLeafPos;
							rLeaf.cpCont = crLeaf.GetContentPointer();
							rLeaf.distSq = cDistSq;
						}
						else
						{
							SInterpolRetrievalType& rNextLeaf = initialLeaves[nextSliceIndex];
							if (!rNextLeaf.cpCont /* || cDistSq < rLeaf.distSq*/)
							{
								rNextLeaf.pos = crLeafPos;
								rNextLeaf.cpCont = crLeaf.GetContentPointer();
								rNextLeaf.distSq = cDistSq;
							}
						}
					}
				}
			}
			leafIndex = crLeaf.GetNextCellLeaf();
			if (leafIndex == TLeaf::INVALID_LEAF_INDEX)
				break;
		}
	}
	if (traversedNodes.empty())
		return 0;
	//now determine all neighboring cells still needed (for the missing quadrants)
	TNeighborCellsType neighbourCells;
	//gather all neighbor cells for the missing quadrants
	const uint32 cCount = traversedNodes.size();
	for (int i = cCount - 1; i >= 0; --i)
		GetRadiusNodeCellsFromTraversal(neighbourCells, traversedNodes[i], crPos, cMaxDistSq);
	//gather for all missing quadrants the leafs from the neighbor cells
	PropagateNeighbourLeaves(neighbourCells, &initialLeaves[0], crOffsetPos, cMaxDistSq);
	//now choose the proper samples for interpolation
	return OptimizeInterpolationNodes(pLeaves, initialLeaves, cMaxDistSq);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::OptimizeInterpolationNodes
(
  TInitialInterpolRetrievalType pFinalNodes,
  const TInitialInterpolRetrievalType cpInitialNodes,
  const float cMaxDistSq
) const
{
	TIndexType leafCount = 0;  //number of leaves added to pFinalNodes
	//take at least one sample every scInitalSliceGranularity/4 samples (to have one per quadrant)
	//neighbour slice is taken if distance is between distance and 2 times it, if closer than current, drop last sample
	int lastSetIndex = -1;        //keeps track of last set sample
	int lastButOneSetIndex = -1;  //keeps track of last but one set sample
	float lastDist = -1.f;        //distance of last set sample
	float queryDist = cMaxDistSq; //distance the sample must have to be added, times 2.f every ignored neighbour cell
	const uint32 cMaxIndexDiff = scInitalSliceGranularity >> 2;//max distance between 2 indices to get at least one sample per quadrant
	bool wasForcedIndex = false;  //if true, last sample was inserted due to too large index (cMaxIndexDiff)
	for (uint32 i = 0; i < scInitalSliceGranularity; ++i)
	{
		const SInterpolRetrievalType& crNode = cpInitialNodes[i];
		if (crNode.cpCont)
		{
			const float cCurDist = crNode.distSq;
			const bool cCloserInBetween = (cCurDist < queryDist);
			const bool cIndexTooLarge = ((i - lastSetIndex) >= cMaxIndexDiff);
			const float cQueryDistRevert = cCurDist * (float)((uint64)1 << (uint64)(i - lastSetIndex - 1));//revert query dist needed for replacement query
			const bool cCloserReplace = (lastDist > cQueryDistRevert);
			if (cCloserReplace && !wasForcedIndex)
			{
				//insert sample into slot of previous one
				assert(leafCount > 0);
				pFinalNodes[leafCount - 1] = crNode;
				wasForcedIndex = ((i - lastButOneSetIndex) >= cMaxIndexDiff);
				lastSetIndex = i;
				lastDist = cCurDist;
				queryDist = std::max(2.f, cCurDist);  //keep some useful min distance
			}
			else if (cIndexTooLarge || cCloserInBetween)//insert sample without unsetting last one
			{
				pFinalNodes[leafCount++] = crNode;
				lastButOneSetIndex = lastSetIndex;
				lastSetIndex = i;
				lastDist = cCurDist;
				queryDist = std::max(2.f, cCurDist);  //keep some useful min distance
				wasForcedIndex = cIndexTooLarge;
			}
		}
		queryDist *= 2.f;
	}
	return leafCount;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetLeafByPos(const TLeafContent*& rpCont, const TVec2& crPos) const
{
	SQuadNodeData nodeData;
	TTravRecVecType traversedNodes;
	uint16 subDivLevel = 0;
	RetrieveDeepestNode(crPos, (float)m_SideLength, m_Center, nodeData, 0, traversedNodes, subDivLevel);
	const SQuadNode& crParentNode = GetNode(nodeData.nodeIndex);
	TIndexType leafIndex = (crParentNode.HasLeafNodes()) ? crParentNode.GetLeafIndex() : TLeaf::INVALID_LEAF_INDEX;
	if (leafIndex != TLeaf::INVALID_LEAF_INDEX)
	{
		for (;; )
		{
			const TLeaf& crLeaf = GetLeaf(leafIndex);
			if (crLeaf.GetPos() == crPos)
			{
				rpCont = crLeaf.GetContentPointer();
				return;
			}
			leafIndex = crLeaf.GetNextCellLeaf();
			if (leafIndex == TLeaf::INVALID_LEAF_INDEX)
				break;
		}
	}
	rpCont = NULL;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::GetMemoryUsage(ICrySizer* pSizer)
{
	pSizer->AddContainer(m_Leaves);
	pSizer->AddContainer(m_Nodes);
}

//functions from former orientation policy
template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TCompType, class TCenterCompType>
inline const float CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetMinDistToQuadSq(const float cSideLength, const SVec2<TCenterCompType>& crCenter, const SVec2<TCompType>& crPos) const
{
	//idea: find out on which corner of cell and then let fall a perpendicular
	//if perpendicular is outside edge, snap it to the next corner point and compute distance to pos
	SVec2<TCompType> cellPosPerpend(min((TCompType)(crCenter.x + cSideLength / 2.0f), crPos.x), min((TCompType)(crCenter.y + cSideLength / 2.0f), crPos.y));
	cellPosPerpend.y = max((TCompType)(crCenter.y - cSideLength / 2.0f), cellPosPerpend.y);
	cellPosPerpend.x = max((TCompType)(crCenter.x - cSideLength / 2.0f), cellPosPerpend.x);
	return cellPosPerpend.GetDistSq(crPos);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TCenterCompType>
inline const float CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetMinDistToQuadSq(const float cSideLength, const SVec2<TCenterCompType>& crCenter, const SVec2<TCenterCompType>& crPos) const
{
	//idea: find out on which corner of cell and then let fall a perpendicular
	//if perpendicular is outside edge, snap it to the next corner point and compute distance to pos
	SVec2<TCenterCompType> cellPosPerpend(min((crCenter.x + cSideLength / 2.0f), crPos.x), min((crCenter.y + cSideLength / 2.0f), crPos.y));
	cellPosPerpend.y = max((crCenter.y - cSideLength / 2.0f), cellPosPerpend.y);
	cellPosPerpend.x = max((crCenter.x - cSideLength / 2.0f), cellPosPerpend.x);
	return cellPosPerpend.GetDistSq(crPos);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TCompType, class TCenterCompType>
inline const float CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetMinDistToQuad(const float cSideLength, const SVec2<TCenterCompType>& crCenter, const SVec2<TCompType>& crPos) const
{
	return sqrtf(GetMinDistToQuadSq(cSideLength, crCenter, crPos));
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TCompType>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetSubdivisionCenters(const float cSideLength, const SVec2<TCompType>& crCenter, SVec2<TCompType> childCenters[4]) const
{
	//processed in enum order
	assert(cSideLength > 0);
	const TCompType cNewHalfSideLength = cSideLength / 4.0;
	childCenters[0].x = childCenters[2].x = (crCenter.x - cNewHalfSideLength);
	childCenters[1].x = childCenters[3].x = (crCenter.x + cNewHalfSideLength);
	childCenters[0].y = childCenters[1].y = (crCenter.y - cNewHalfSideLength);
	childCenters[2].y = childCenters[3].y = (crCenter.y + cNewHalfSideLength);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TCompType, class TCenterCompType>
inline const TEChildIndex CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::
GetQuadrantIndex(const SVec2<TCenterCompType>& crCenterPos, const SVec2<TCompType>& crPos) const
{
	return ((TCenterCompType)crPos.y < crCenterPos.y) ? ((TCenterCompType)crPos.x >= crCenterPos.x) ? AS : SS :
	       ((TCenterCompType)crPos.x >= crCenterPos.x) ? AA : SA;
}
};//NQT

#pragma warning(default:4127)

