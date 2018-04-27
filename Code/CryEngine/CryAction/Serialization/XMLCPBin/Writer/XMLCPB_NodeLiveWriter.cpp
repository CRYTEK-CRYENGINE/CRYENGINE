// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_NodeLiveWriter.h"
#include "XMLCPB_Writer.h"

using namespace XMLCPB;

const int CNodeLiveWriter::INITIAL_SIZE_CHILDREN_VECTOR = 4;
const int CNodeLiveWriter::INITIAL_SIZE_ATTRS_VECTOR = 4;

#ifdef XMLCPB_COLLECT_STATS
CNodeLiveWriter::TStats CNodeLiveWriter::m_stats;
#endif

/////////////////
void CNodeLiveWriterRef::CopyFrom(const CNodeLiveWriterRef& other)
{
	m_nodeId = other.m_nodeId;
	m_pNode_Debug = other.m_pNode_Debug;
	m_safecheckID = other.m_safecheckID;

	if (m_nodeId != XMLCPB_INVALID_ID)
	{
		CNodeLiveWriter* pNode = m_Writer.GetNodeLive(m_nodeId);
		if (!pNode->IsValid() || pNode->GetSafeCheckID() != m_safecheckID)
		{
			assert(false);
			m_nodeId = XMLCPB_INVALID_ID;
			m_pNode_Debug = NULL;
		}
	}
}

///////////////
CNodeLiveWriter* CNodeLiveWriterRef::GetNode() const
{
	if (m_nodeId == XMLCPB_INVALID_ID)
		return NULL;

	CNodeLiveWriter* pNode = m_Writer.GetNodeLive(m_nodeId);

	if (!pNode->IsValid() || pNode->GetSafeCheckID() != m_safecheckID)
	{
		assert(false);
		return NULL;
	}

	return pNode;
}

/////////////////
CNodeLiveWriterRef::CNodeLiveWriterRef(CWriter& Writer, NodeLiveID nodeId)
	: m_Writer(Writer)
{
	m_nodeId = nodeId;
	m_pNode_Debug = NULL;
	if (m_nodeId == XMLCPB_INVALID_ID)
		return;

	CNodeLiveWriter* pNode = m_Writer.GetNodeLive(m_nodeId);

	if (!pNode->IsValid())
	{
		assert(false);
		return;
	}

	m_pNode_Debug = pNode;
	m_safecheckID = pNode->GetSafeCheckID();
}

/////////////////
bool CNodeLiveWriterRef::IsValid() const
{
	if (m_nodeId == XMLCPB_INVALID_ID)
		return false;

	CNodeLiveWriter* pNode = m_Writer.GetNodeLive(m_nodeId);
	return (pNode->IsValid() && pNode->GetSafeCheckID() == m_safecheckID);
}

//////////////////////////////////////////////////////////////////////////

CNodeLiveWriter::CNodeLiveWriter(CWriter& Writer, NodeLiveID ID, StringID IDTag, uint32 safecheckID)
	: m_Writer(Writer)
	, m_ID(ID)
{
	m_attrs.reserve(INITIAL_SIZE_ATTRS_VECTOR);
	m_children.reserve(INITIAL_SIZE_CHILDREN_VECTOR);
	Reuse(IDTag, safecheckID);
}

//////////////////////////////////////////////////////////////////////////

CNodeLiveWriterRef CNodeLiveWriter::AddChildNode(const char* pChildName)
{
	assert(!m_done && m_valid);

	m_totalAmountChildren++;

	// only 1 child can be not done at any time
	if (!m_children.empty())
	{
		CNodeLiveWriter* pPreviousChild = m_Writer.GetNodeLive(m_children.back());
		if (!pPreviousChild->IsDone())
			pPreviousChild->Done();
	}

	assert(m_children.size() <= CHILDREN_PER_BLOCK);
	if (m_children.size() == CHILDREN_PER_BLOCK)
		CompactPendingChildren();

	//
	CNodeLiveWriter* pChild = m_Writer.CreateAndAddLiveNode(pChildName);

	m_children.push_back(pChild->GetID());

	pChild->SetParent(GetID());

	return CNodeLiveWriterRef(m_Writer, pChild->GetID());
}

//////////////////////////////////////////////////////////////////////////

void CNodeLiveWriter::SetParent(NodeLiveID ID)
{
	assert(m_IDParent == XMLCPB_INVALID_ID);
	m_IDParent = ID;
}

//////////////////////////////////////////////////////////////////////////
// "Done" node is a node that can not be modified anymore, or added more children. Is only waiting for when is the time to be compacted into the main buffer, and discarded.

void CNodeLiveWriter::Done()
{
	assert(!m_done && m_valid);
	m_done = true;

	int numChildren = m_children.size();
	for (int i = 0; i < numChildren; ++i)
	{
		CNodeLiveWriter* pChild = m_Writer.GetNodeLive(m_children[i]);
		if (!pChild->IsDone())
			pChild->Done();
	}

	if (!m_childrenAreCompacted)
	{
		CompactPendingChildren();
		m_childrenAreCompacted = true;
	}

	if (m_IDParent != XMLCPB_INVALID_ID)
	{
		CNodeLiveWriter* pParent = m_Writer.GetNodeLive(m_IDParent);
		pParent->ChildIsDone();
	}
	else
	{
		Compact();
	}
}

//////////////////////////////////////////////////////////////////////////

void CNodeLiveWriter::ChildIsDone()
{
	// if all children done, and this is done too, compact the children
	if (m_done)
	{
		CompactPendingChildren();
		m_childrenAreCompacted = true;
	}
}

//////////////////////////////////////////////////////////////////////////

void CNodeLiveWriter::CompactPendingChildren()
{
	int numChildren = m_children.size();
	if (numChildren == 0)
		return;

	for (int i = 0; i < numChildren; ++i)
	{
		CNodeLiveWriter* pChild = m_Writer.GetNodeLive(m_children[i]);
		pChild->Compact();
	}
	m_globalIdLastChildInBlock.push_back(m_Writer.GetLastUsedGlobalId());    // the global id is just the order of the node in the final big buffer
	m_children.clear();
}

//////////////////////////////////////////////////////////////////////////

void CNodeLiveWriter::AddAttr(const char* pAttrName, const uint8* data, uint32 len, bool needInmediateCopy)
{
	assert(data && len > 0);
	assert(!m_done && m_valid);

	CAttrWriter attr(m_Writer);
	attr.Set(pAttrName, data, len, needInmediateCopy);

	m_attrs.push_back(attr);
}

//////////////////////////////////////////////////////////////////////////

const char* CNodeLiveWriter::GetTag() const
{
	return m_Writer.GetTagsTable().GetString(m_IDTag);
}

//////////////////////////////////////////////////////////////////////////

void CNodeLiveWriter::Reuse(StringID IDTag, uint32 safecheckID)
{
	m_IDTag = IDTag;
	m_IDParent = XMLCPB_INVALID_ID;
	m_done = false;
	m_valid = true;
	m_childrenAreCompacted = false;
	m_children.resize(0);
	m_attrs.clear();
	m_totalAmountChildren = 0;
	m_safecheckID = safecheckID;
	m_globalIdLastChildInBlock.resize(0);
}

//////////////////////////////////////////////////////////////////////////
// compacting is the process of convert the node information into final binary data, and store it into the writer main buffer.
// binary format for a node:
// 2 bytes  -> header, split into:
//							       15 -> 1 = has attrs
//										 14 -> 1 = children are right before
//										 13 ->    -| 0-2 number of children. 3 = more than 2 children
//										 12 ->    -|
//										 11 ->  -|
//										 10 ->  -| 2 bits high for the attrSet id
//									-> 10 bits  (9-0)  -> Tag ID
//
// 1 byte				-> children.
//               bit 7 ->  1 = more than 63 children.
//                  -- when bit7 ==1, bit0-6 are the high part of the number
//                  -- when bit7 ==0:
//										bit 6 -> 1 = compact form. 0 = bits0-5 are number of children
//										-- when bit6 == 1:
//                        bits 0-2  -> number of children
//                        bits 3-5  -> distance of the last children from this node (in nodes)
// 1 byte				-> children. Only used if there are more than 127 children. In that case, this is the 8 lower bits, the previous byte is the 7 higher bits
//
// 1/0			    -> 8 lower bits for the attrSet id (only if has attrs )
// 2/0          -> when the final attrSetId is >1022, it is stored in 16 bits here. In that case, the previous byte is 255 and the 2 bits in the header are both 1.
//
// [..]        -> array of children block. Each entry is the distance, in nodes, from this one to the last children of the block.
//                when there is only 1 children block, it can be stored in 8 or 16 bits. When there are more than 1 block, each entry is always 24 bits.
//                -- first byte:
//                      bit 7 -> 0 = 8 bits. in this case, bits 0-6 is the dist.
//                      if bit7 ==1 then:
//                           bit 6 -> 0 = 16 bits. in this case, 0-5 is the high bits, the next byte is the lower bits.
//                              if bit6 ==1 then:
//                                 bits 0-5 is the high bits. the next 2 bytes are the lower bits, stored as a single uint16
//
//
// [...]									-> attr data

void CNodeLiveWriter::Compact()
{
	assert(m_done);

	CBufferWriter& writeBuffer = m_Writer.GetMainBuffer();

	if (m_IDParent == XMLCPB_INVALID_ID)
		m_Writer.NotifyRootNodeStartCompact();

#ifdef XMLCPB_COLLECT_STATS
	uint32 startSize = writeBuffer.GetDataSize();
#endif

	NodeGlobalID lastGlobalId = m_Writer.GetLastUsedGlobalId();
	uint32 numChildBlocks = m_globalIdLastChildInBlock.size();

	assert(m_IDTag <= MAX_TAGID);
	uint16 nodeHeader = m_IDTag;

	static_assert(BITS_TAGID + HIGHPART_ATTRID_NUM_BITS + CHILDREN_HEADER_BITS <= LOWEST_BIT_USED_AS_FLAG_IN_NODE_HEADER, "Unexpected values!");

	uint32 numAttrs = m_attrs.size();
	assert(numAttrs <= MAX_NUM_ATTRS);

#ifdef XMLCPB_CHECK_HARDCODED_LIMITS
	CheckHardcodedLimits();
#endif

	// attrSetId madness. specified in a few bits in the header + 8 bits outside, but sometimes need 16 extra bits
	AttrSetID attrSetIdReal = XMLCPB_INVALID_ID;
	AttrSetID attrSetId8Bits = XMLCPB_INVALID_ID;
	uint8 attrSetLow8Bits = 0;
	if (numAttrs > 0)
	{
		nodeHeader |= FLN_HASATTRS;

		uint16 attrHeaders[MAX_NUM_ATTRS];
		for (int a = 0; a < numAttrs; ++a)
			attrHeaders[a] = m_attrs[a].CalcHeader();

		CAttrSet set;
		set.m_numAttrs = numAttrs;
		set.m_pHeaders = &attrHeaders[0];

		attrSetIdReal = m_Writer.GetAttrSetTable().GetSetID(set);
		attrSetId8Bits = attrSetIdReal > ATTRID_MAX_VALUE_8BITS ? ATRID_USING_16_BITS : attrSetIdReal;

		attrSetLow8Bits = attrSetId8Bits;
		uint32 highBitsAttrSetId = (attrSetId8Bits << (HIGHPART_ATTRID_BIT_POSITION - 8)) & HIGHPART_ATTRID_MASK;
		nodeHeader |= highBitsAttrSetId;
	}

	// amount of children is specified in a few bits in the header, when that is possible
	{
		uint32 val = m_totalAmountChildren;
		if (m_totalAmountChildren > CHILDREN_HEADER_MAXNUM)
			val = CHILDREN_HEADER_CANTFIT;
		val = val << CHILDREN_HEADER_BIT_POSITION;
		nodeHeader |= val;
	}

	bool childrenRightBefore = numChildBlocks == 1 && m_globalIdLastChildInBlock[0] == lastGlobalId;
	if (childrenRightBefore)
		nodeHeader |= FLN_CHILDREN_ARE_RIGHT_BEFORE;

	// write node header, in case you didnt notice!
	writeBuffer.AddData(&nodeHeader, sizeof(nodeHeader));

	bool childrenAndDistStoredInFirstByte = false;

	// add num children
	if (m_totalAmountChildren > MAX_NUMBER_OF_CHILDREN_IN_8BITS)  // when the number of children does not fit in 8 bits
	{
		assert(m_totalAmountChildren < MAX_NUMBER_OF_CHILDREN_IN_16BITS);
		uint8 childrenLow = m_totalAmountChildren & 255;
		uint8 childrenHigh = m_totalAmountChildren >> 8;
		childrenHigh |= FL_CHILDREN_NUMBER_IS_16BITS;
		writeBuffer.AddData(&childrenHigh, sizeof(childrenHigh));
		writeBuffer.AddData(&childrenLow, sizeof(childrenLow));
	}
	else if (m_totalAmountChildren > CHILDREN_HEADER_MAXNUM)  // when the number of children does not fit in the header, but it does fit in 8 bits
	{
		// tries to store both the number of children and the distance to them in one single byte.
		uint32 children = m_totalAmountChildren - CHILDREN_HEADER_MAXNUM; // dirty trick: subtracts the value that could fit in the header, to increase a bit the range of valid values for the all-in-single-byte attempt.

		if (children <= CHILDANDDISTINONEBYTE_MAX_AMOUNT_CHILDREN && !childrenRightBefore)
		{
			assert(lastGlobalId > m_globalIdLastChildInBlock[0]);
			uint32 distNodes = (lastGlobalId - m_globalIdLastChildInBlock[0]) - 1; // dirty trick: -1 because, as it cant never be 0, by doing that we can extend the range a little bit
			if (distNodes <= CHILDANDDISTINONEBYTE_MAX_DIST)
			{
				uint8 val = children | (distNodes << CHILDANDDISTINONEBYTE_CHILD_BITS) | FL_CHILDREN_NUMBER_AND_BLOCKDIST_IN_ONE_BYTE;
				childrenAndDistStoredInFirstByte = true;
				writeBuffer.AddData(&val, sizeof(val));
			}
		}
		if (!childrenAndDistStoredInFirstByte)
		{
			uint8 val = m_totalAmountChildren;
			writeBuffer.AddData(&val, sizeof(val));
		}
	}

	// add 8 lower bits of the attrSetId, in case there are attrs, that is. And a full value in 16 bits in case it does not fit. (still need the 8 bits because they mark that situation with an special value)
	if (numAttrs > 0)
	{
		writeBuffer.AddData(&attrSetLow8Bits, sizeof(attrSetLow8Bits));
		if (attrSetIdReal > ATTRID_MAX_VALUE_8BITS)
		{
			uint16 val16 = attrSetIdReal;
			assert(attrSetIdReal < 65536);
			writeBuffer.AddData(&val16, sizeof(val16));
		}
	}

	// addr child blocks
	if (!childrenRightBefore && !childrenAndDistStoredInFirstByte)
	{
		bool isChildBlockSaved = false;
		if (numChildBlocks == 1)
		{
			uint32 dist = lastGlobalId - m_globalIdLastChildInBlock[0];

			if (dist <= CHILDBLOCKS_MAX_DIST_FOR_8BITS)
			{
				uint8 byteDist = dist;
				writeBuffer.AddData(&byteDist, sizeof(byteDist));
				isChildBlockSaved = true;
			}
			else if (dist <= CHILDBLOCKS_MAX_DIST_FOR_16BITS)
			{
				uint8 byteHigh = CHILDBLOCKS_USING_MORE_THAN_8BITS | (dist >> 8);
				uint8 byteLow = dist & 255;
				writeBuffer.AddData(&byteHigh, sizeof(byteHigh));
				writeBuffer.AddData(&byteLow, sizeof(byteLow));
				isChildBlockSaved = true;
			}
		}

		if (!isChildBlockSaved)  // can happen either if has only 1 block and the dist cant fit in 8-16 bits, or because it has more than 1 block of children
		{
			for (int i = 0; i < numChildBlocks; ++i)
			{
				uint32 dist = lastGlobalId - m_globalIdLastChildInBlock[i];
				uint8 byteHigh = CHILDBLOCKS_USING_24BITS | (dist >> 16);
				uint16 wordLow = dist & 65535;
				writeBuffer.AddData(&byteHigh, sizeof(byteHigh));
				writeBuffer.AddData(&wordLow, sizeof(wordLow));
			}
		}
	}

#ifdef XMLCPB_COLLECT_STATS
	uint32 endSize = writeBuffer.GetDataSize();
	if (m_Writer.IsSavingIntoFile())
	{
		m_stats.m_totalSizeNodeData += (endSize - startSize);
		startSize = endSize; // to calc the attrs
	}
#endif

	for (int a = 0; a < m_attrs.size(); ++a)
	{
		m_attrs[a].Compact();
	}

#ifdef XMLCPB_COLLECT_STATS
	if (m_Writer.IsSavingIntoFile())
	{
		endSize = writeBuffer.GetDataSize();
		m_stats.m_totalSizeAttrData += (endSize - startSize);

		m_stats.m_maxNumChildren = max(m_stats.m_maxNumChildren, m_totalAmountChildren);
		m_stats.m_maxNumAttrs = max(m_stats.m_maxNumAttrs, m_attrs.size());
		m_stats.m_totalAttrs += m_attrs.size();
		m_stats.m_totalChildren += m_totalAmountChildren;
		m_stats.m_totalNodesCreated++;
	}
#endif

	m_valid = false;
	m_Writer.NotifyNodeCompacted();
	m_Writer.FreeNodeLive(GetID());
}

//////////////////////////////////////////////////////////////////////////
// used only for debug purposes
const char* CNodeLiveWriter::ReadAttrStr(const char* pAttrName)
{
	for (int i = 0; i < m_attrs.size(); ++i)
	{
		if (strcmp(m_attrs[i].GetName(), pAttrName) == 0)
		{
			return m_attrs[i].GetStrData();
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// used only for debug purposes
bool CNodeLiveWriter::HaveAttr(const char* pAttrName)
{
	for (int i = 0; i < m_attrs.size(); ++i)
	{
		if (strcmp(m_attrs[i].GetName(), pAttrName) == 0)
		{
			return true;
		}
	}

	return false;
}

#ifdef XMLCPB_CHECK_HARDCODED_LIMITS

//////////////////////////////////////////////////////////////////////////
void CNodeLiveWriter::CheckHardcodedLimits()
{
	if (!m_Writer.HasInternalError())
	{
		uint32 numAttrs = m_attrs.size();
		if (m_IDTag > MAX_TAGID)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: IDTag overflow: %u / %d", m_IDTag, MAX_TAGID);
			ShowExtendedErrorInfo();
			m_Writer.NotifyInternalError();
		}
		else if (numAttrs > MAX_NUM_ATTRS)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: NumAttrs overflow: %u / %d", numAttrs, MAX_NUM_ATTRS);
			ShowExtendedErrorInfo();
			m_Writer.NotifyInternalError();
		}
		else if (m_totalAmountChildren > MAX_NUMBER_OF_CHILDREN_IN_16BITS)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: NumChildren overflow: %u / %d", m_totalAmountChildren, MAX_NUMBER_OF_CHILDREN_IN_16BITS);
			ShowExtendedErrorInfo();
			m_Writer.NotifyInternalError();
		}
		else
		{
			for (uint32 a = 0; a < numAttrs; ++a)
			{
				m_attrs[a].CheckHardcodedLimits(a);
				if (m_Writer.HasInternalError())
				{
					ShowExtendedErrorInfo();
					break;
				}
			}
		}
	}
}

void CNodeLiveWriter::ShowExtendedErrorInfo() const
{
	const CNodeLiveWriter* pNode = this;
	std::vector<const CNodeLiveWriter*> nodesVec;
	do
	{
		nodesVec.push_back(pNode);
		if (pNode->m_IDParent != XMLCPB_INVALID_ID)
			pNode = m_Writer.GetNodeLive(pNode->m_IDParent);
	}
	while (pNode->m_IDParent != XMLCPB_INVALID_ID);

	string buf;
	for (int i = nodesVec.size() - 1; i >= 0; --i)
	{
		const CNodeLiveWriter* pNodeInt = nodesVec[i];
		buf.append(m_Writer.GetTagsTable().GetStringSafe(pNodeInt->m_IDTag));
		if (i != 0)
			buf.append("->");
	}

	CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Node: %s", buf.c_str());
}
#endif
