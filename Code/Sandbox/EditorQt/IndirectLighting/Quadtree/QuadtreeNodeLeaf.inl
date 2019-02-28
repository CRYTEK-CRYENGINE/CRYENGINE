// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   quadtree class implementations for node and leaf
 */

namespace NQT
{
#if defined(_DEBUG)
//	template <class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
//	inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SClosestContentStat::SClosestContentStat()
//		: leafArrayAccesses(0), nodeArrayAccesses(0){}
//
//	template <class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
//	inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SClosestContentStat::Reset()
//	{
//		leafArrayAccesses = nodeArrayAccesses = 0;
//	}
//
//	template <class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
//	inline const typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SClosestContentStat
//	CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::GetDebugStats() const
//	{
//#if defined(_DEBUG)
//		assert(m_IsInitialized == true);
//#endif
//		return m_DebugStats;
//	}
#endif

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafSort::
SQuadLeafSort(const typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::TLeaf*& crpLeaf, const float cDistSq)
	: pLeaf(crpLeaf), distSq(cDistSq)
{}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::SQuadLeafSort::
operator<(const SQuadLeafSort& crLeafSort) const
{
	return distSq < crLeafSort.distSq;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SContentSort::
SContentSort(TLeafContent* cpCont, const TVec2& crPos, const float cDistSq)
	: contRetrieval(std::make_pair(cpCont, crPos)), distSq(cDistSq)
{}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius >::SContentSort::operator<(const SContentSort& crLeafSort) const
{
	return (distSq < crLeafSort.distSq);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::
SQuadLeaf(const TLeafContentQuad& crCont, const TVec2& crPos, const TRadiusType) : pos(crPos), cont(crCont), nextLeafIndex(0){}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
const size_t CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::
GetSerializationSize(const size_t cContentSerializationSize)
{
	return (sizeof(TVec2) + sizeof(TIndexTypeQuad) + cContentSerializationSize);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::SQuadLeaf()
	: nextLeafIndex(0){}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline const TIndexTypeQuad CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::GetNextCellLeaf() const
{
	if (nextLeafIndex & HAS_MORE_LEAVES_INDEX)
		return (nextLeafIndex & ~HAS_MORE_LEAVES_INDEX);
	else
		return INVALID_LEAF_INDEX;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline const TLeafContentQuad &CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::GetContent() const
{
	return cont;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline const TLeafContentQuad * CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::GetContentPointer() const
{
	return &cont;    //needed for retrieval to not copy content multiple times
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::SetContent(const TLeafContentQuad &crCont)
{
	cont = crCont;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline const SVec2<TPosType> &CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::GetPos() const
{
	return pos;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::SetPos(const TVec2 &crPos)
{
	pos = crPos;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline const typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::TRadiusType
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::GetRadius() const
{
	return 0;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::SetNextCellLeaf(const TIndexTypeQuad cNextIndex, const bool cHasNextLeaf)
{
	assert((cNextIndex & ~HAS_MORE_LEAVES_INDEX) == cNextIndex);  //flag should not be set here, might be an error
	nextLeafIndex = cHasNextLeaf ? (HAS_MORE_LEAVES_INDEX | cNextIndex) : 0;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::
Serialize(SerializeContentFunction pSerializeFnct, const size_t cContentSerializationSize, uint8 * &pStreamPos) const
{
	const TLeafContentQuad& crCont = GetContent();
	pSerializeFnct(crCont, pStreamPos);
	pStreamPos += cContentSerializationSize;
	const TVec2& crPos = GetPos();
	memcpy(pStreamPos, &crPos, sizeof(TVec2));
	pStreamPos += sizeof(TVec2);
	memcpy(pStreamPos, &nextLeafIndex, sizeof(nextLeafIndex));
	pStreamPos += sizeof(nextLeafIndex);
}

#if defined(__GNUC__)
template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::Clone() const
{
	return SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>(cont, pos, 0);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>::Clone() const
{
	return SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>(this->cont, this->pos, this->radius);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>::SQuadLeafRadius()
	: SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>(), radius(0){}
#else
template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::Clone() const
{
	return SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>(cont, pos, 0);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>::Clone() const
{
	return SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>(cont, pos, radius);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>::SQuadLeafRadius()
	: SQuadLeaf(), radius(0){}
#endif

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>::
SQuadLeafRadius(const TLeafContentQuad& crCont, const TVec2& crPos, const TRadiusType cRadius)
	: SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>(crCont, crPos, 0), radius(cRadius){}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
inline const typename CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::TRadiusType
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>::GetRadius() const
{
	return radius;
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
const size_t CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>::
GetSerializationSize(const size_t cContentSerializationSize)
{
	return (sizeof(TRadiusType) + SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::GetSerializationSize(cContentSerializationSize));
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
template<class TLeafContentQuad, class TIndexTypeQuad>
void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadLeafRadius<TLeafContentQuad, TIndexTypeQuad>::
Serialize(SerializeContentFunction pSerializeFnct, const size_t cContentSerializationSize, uint8 * &pStreamPos) const
{
	SQuadLeaf<TLeafContentQuad, TIndexTypeQuad>::Serialize(pSerializeFnct, cContentSerializationSize, pStreamPos);
	const TRadiusType cRadius = GetRadius();
	memcpy(pStreamPos, &cRadius, sizeof(TRadiusType));
	pStreamPos += sizeof(TRadiusType);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNodeData::SQuadNodeData() : nodeIndex(0){}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::
SQuadNodeData::SQuadNodeData(const TIndexType cNodeIndex, const TVec2F& crCenterPos, const float cSideLength)
	: nodeIndex(cNodeIndex), centerPos(crCenterPos), sideLength(cSideLength){}

//template <class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
//inline CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNode::SQuadNode()
//: childNodeIndex(0){}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNode::SetIsLeaf(const bool cIsLeaf)
{
	if (cIsLeaf)
		leafIndex |= IS_LEAF_INDEX;
	else
		leafIndex &= ~IS_LEAF_INDEX;
}

//template <class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
//inline const TIndexType CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNode::GetLeafIndex() const
//{
//	assert(HasLeafNodes());
//	return (leafIndex & ~IS_LEAF_INDEX);
//}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNode::SetLeafIndex(const TIndexType cIndex)
{
	assert(HasLeafNodes());
	leafIndex = (IS_LEAF_INDEX | cIndex);
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNode::HasLeafNodes() const
{
	return ((leafIndex & IS_LEAF_INDEX) != 0);  //return true if leaf bit is set
}

template<class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
inline void CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNode::SwapEndianess()
{
	SwapEndian(childNodeIndex);
}

//template <class TLeafContent, uint32 TMaxCellElems, class TPosType, class TIndexType, bool TUseRadius>
//inline const bool CQuadTree<TLeafContent, TMaxCellElems, TPosType, TIndexType, TUseRadius>::SQuadNode::HasChildNodes() const
//{
//	return ((leafIndex & ~IS_LEAF_INDEX) != 0 && (leafIndex & IS_LEAF_INDEX) == 0);//return true if index is not 0 and leaf bit is not set
//}
};//NQT

